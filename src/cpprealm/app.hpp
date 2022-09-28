////////////////////////////////////////////////////////////////////////////
//
// Copyright 2022 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef realm_app_hpp
#define realm_app_hpp

#include <realm/object-store/sync/app.hpp>
#include <realm/object-store/sync/sync_user.hpp>
#include <realm/object-store/sync/impl/sync_client.hpp>

#include <cpprealm/type_info.hpp>
#include <cpprealm/task.hpp>
#include <cpprealm/db.hpp>
#include <utility>

#if !__APPLE__
#include <curl/curl.h>
#endif
namespace realm {

namespace internal {

#if !__APPLE__
class CurlGlobalGuard {
public:
    CurlGlobalGuard()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (++m_users == 1) {
            curl_global_init(CURL_GLOBAL_ALL);
        }
    }

    ~CurlGlobalGuard()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (--m_users == 0) {
            curl_global_cleanup();
        }
    }

    CurlGlobalGuard(const CurlGlobalGuard&) = delete;
    CurlGlobalGuard(CurlGlobalGuard&&) = delete;
    CurlGlobalGuard& operator=(const CurlGlobalGuard&) = delete;
    CurlGlobalGuard& operator=(CurlGlobalGuard&&) = delete;

private:
    static std::mutex m_mutex;
    static int m_users;
};

std::mutex CurlGlobalGuard::m_mutex = {};
int CurlGlobalGuard::m_users = 0;

size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, std::string* response)
{
    REALM_ASSERT(response);
    size_t realsize = size * nmemb;
    response->append(ptr, realsize);
    return realsize;
}

size_t curl_header_cb(char* buffer, size_t size, size_t nitems, std::map<std::string, std::string>* response_headers)
{
    REALM_ASSERT(response_headers);
    std::string combined(buffer, size * nitems);
    if (auto pos = combined.find(':'); pos != std::string::npos) {
        std::string key = combined.substr(0, pos);
        std::string value = combined.substr(pos + 1);
        while (value.size() > 0 && value[0] == ' ') {
            value = value.substr(1);
        }
        while (value.size() > 0 && (value[value.size() - 1] == '\r' || value[value.size() - 1] == '\n')) {
            value = value.substr(0, value.size() - 1);
        }
        response_headers->insert({key, value});
    }
    else {
        if (combined.size() > 5 && combined.substr(0, 5) != "HTTP/") { // ignore for now HTTP/1.1 ...
            std::cerr << "test transport skipping header: " << combined << std::endl;
        }
    }
    return nitems * size;
}
app::Response do_http_request(const app::Request& request)
{
    CurlGlobalGuard curl_global_guard;
    auto curl = curl_easy_init();
    if (!curl) {
        return app::Response{500, -1};
    }

    struct curl_slist* list = nullptr;
    auto curl_cleanup = util::ScopeExit([&]() noexcept {
        curl_easy_cleanup(curl);
        curl_slist_free_all(list);
    });

    std::string response;
    std::map<std::string, std::string> response_headers;

    /* First set the URL that is about to receive our POST. This URL can
     just as well be a https:// URL if that is what should receive the
     data. */
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());

    /* Now specify the POST data */
    if (request.method == app::HttpMethod::post) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::put) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::patch) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::del) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::patch) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout_ms);

    for (auto header : request.headers) {
        auto header_str = util::format("%1: %2", header.first, header.second);
        list = curl_slist_append(list, header_str.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);

    auto response_code = curl_easy_perform(curl);
    if (response_code != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed when sending request to '%s' with body '%s': %s\n",
                request.url.c_str(), request.body.c_str(), curl_easy_strerror(response_code));
    }
    int http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    return {
        http_code,
        0, // binding_response_code
        std::move(response_headers),
        std::move(response),
    };
}
#endif
#if !__APPLE__
class DefaultTransport : public app::GenericNetworkTransport {
public:
    void send_request_to_server(app::Request&& request,
                                util::UniqueFunction<void(const app::Response&)>&& completion_block)
    {

        completion_block(do_http_request(request));
    }
};
#else
class DefaultTransport : public app::GenericNetworkTransport {
public:
    void send_request_to_server(app::Request&& request,
                                app::HttpCompletion&& completion_block) override;
};
#endif
} // anonymous namespace

// Represents an error state from the server.
struct app_error {

    app_error(realm::app::AppError error) : m_error(error)
    {
    }

    std::error_code error_code() const
    {
        return m_error.error_code;
    }

    std::string_view mesage() const
    {
        return m_error.message;
    }

    std::string_view link_to_server_logs() const
    {
        return m_error.link_to_server_logs;
    }

    bool is_json_error() const
    {
        return error_code().category() == realm::app::json_error_category();
    }

    bool is_service_error() const
    {
        return error_code().category() == realm::app::service_error_category();
    }

    bool is_http_error() const
    {
        return error_code().category() == realm::app::http_error_category();
    }

    bool is_custom_error() const
    {
        return error_code().category() == realm::app::custom_error_category();
    }

    bool is_client_error() const
    {
        return error_code().category() == realm::app::client_error_category();
    }
private:
    realm::app::AppError m_error;
};

// MARK: User

/**
 A `user` instance represents a single Realm App user account.

 A user may have one or more credentials associated with it. These credentials
 uniquely identify the user to the authentication provider, and are used to sign
 into a MongoDB Realm user account.

 Note that user objects are only vended out via SDK APIs, and cannot be directly
 initialized. User objects can be accessed from any thread.
 */
struct user {
    user() = default;
    user(const user&) = default;
    user(user&&) = default;
    user& operator=(const user&) = default;
    user& operator=(user&&) = default;
    explicit user(std::shared_ptr<SyncUser> user, std::shared_ptr<app::App> app)
    : m_user(std::move(user)), m_app(std::move(app))
    {
    }

    /**
     The state of the user object.
     */
    enum class state : uint8_t {
        logged_out,
        logged_in,
        removed,
    };

    /**
     The unique MongoDB Realm string identifying this user.
     Note this is different from an identitiy: A user may have multiple identities but has a single indentifier. See RLMUserIdentity.
     */
    std::string identifier() const
    {
       return m_user->identity();
    }

    /**
     The current state of the user.
     */
    state state() const
    {
        return static_cast<enum state>(m_user->state());
    }

    /**
     The user's refresh token used to access the Realm Application.

     This is required to make HTTP requests to MongoDB Realm's REST API
     for functionality not exposed natively. It should be treated as sensitive data.
     */
    std::string access_token() const
    {
        return m_user->access_token();
    }

    /**
     The user's refresh token used to access the Realm Applcation.

     This is required to make HTTP requests to the Realm App's REST API
     for functionality not exposed natively. It should be treated as sensitive data.
     */
    std::string refresh_token() const
    {
        return m_user->refresh_token();
    }

    db_config flexible_sync_configuration() const
    {
        db_config config;
        config.sync_config = std::make_shared<SyncConfig>(m_user, SyncConfig::FLXSyncEnabled{});
        config.sync_config->error_handler = [](std::shared_ptr<SyncSession> session, SyncError error) {
            std::cerr<<"sync error: "<<error.message<<std::endl;
        };
        config.path = m_user->sync_manager()->path_for_realm(*config.sync_config);
        config.sync_config->client_resync_mode = realm::ClientResyncMode::Manual;
        config.sync_config->stop_policy = SyncSessionStopPolicy::AfterChangesUploaded;
        return config;
    }

    /**
     Logs out the current user

     The users state will be set to `Removed` is they are an anonymous user or `LoggedOut` if they are authenticated by an email / password or third party auth clients
     If the logout request fails, this method will still clear local authentication state.
    */
    task<void> log_out() {
        try {
            auto error = co_await make_awaitable<util::Optional<app_error>>([&](auto cb) {
                m_app->log_out(m_user, cb);
            });
            if (error) {
                throw *error;
            }
        } catch (std::exception& err) {
            throw;
        }

        co_return;
    }

    /**
     Logs out the current user

     The users state will be set to `Removed` is they are an anonymous user or `LoggedOut` if they are authenticated by an email / password or third party auth clients
     If the logout request fails, this method will still clear local authentication state.
    */
    void log_out(util::UniqueFunction<void(std::optional<app_error>)>&& callback)
    {
        m_app->log_out(m_user, [cb = std::move(callback)](auto error) {
            cb(error ? std::optional<app_error>{app_error(*error)} : std::nullopt);
        });
    }

    /**
     The custom data of the user.
     This is configured in your Atlas App Services app.
    */
    std::optional<bson::BsonDocument> custom_data()
    {
        return m_user->custom_data();
    }

    /**
     Calls the Atlas App Services function with the provided name and arguments.

     @param name The name of the Atlas App Services function to be called.
     @param arguments The `BsonArray` of arguments to be provided to the function.
     @param callback The completion handler to call when the function call is complete.
     This handler is executed on the thread the method was called from.
    */
    void call_function(const std::string& name, const realm::bson::BsonArray& arguments,
                       util::UniqueFunction<void(std::optional<bson::Bson>&&, std::optional<app_error>)> callback)
    {
        m_app->call_function(name, arguments, std::move(callback));
    }

    /**
     Calls the Atlas App Services function with the provided name and arguments.

     @param name The name of the Atlas App Services function to be called.
     @param arguments The `BsonArray` of arguments to be provided to the function.
     @returns An optional Bson object containing the servers response.
    */
    task<std::optional<bson::Bson>> call_function(const std::string& name, const realm::bson::BsonArray& arguments)
    {
        try {
            auto opt_bson = co_await make_awaitable<util::Optional<bson::Bson>>( [&](auto cb) {
                m_app->call_function(m_user, name, arguments, std::move(cb));
            });
            co_return opt_bson;
        } catch (std::exception& err) {
            throw;
        }
    }

    /**
     Refresh a user's custom data. This will, in effect, refresh the user's auth session.
    */
    void refresh_custom_user_data(util::UniqueFunction<void(std::optional<app_error>)> callback)
    {
        m_user->refresh_custom_data(std::move(callback));
    }

    /**
     Refresh a user's custom data. This will, in effect, refresh the user's auth session.
    */
    task<void> refresh_custom_user_data()
    {
        try {
            auto error = co_await make_awaitable<std::optional<app_error>>( [&](auto cb) {
                m_user->refresh_custom_data(std::move(cb));
            });
            if (error) {
                throw *error;
            }
        } catch (std::exception& err) {
            throw;
        }
    }

    std::shared_ptr<app::App> m_app;
    std::shared_ptr<SyncUser> m_user;
};
static_assert((int)user::state::logged_in  == (int)SyncUser::State::LoggedIn);
static_assert((int)user::state::logged_out == (int)SyncUser::State::LoggedOut);
static_assert((int)user::state::removed    == (int)SyncUser::State::Removed);

class App {
    static std::unique_ptr<util::Logger> defaultSyncLogger(util::Logger::Level level) {
        struct SyncLogger : public util::RootLogger {
            void do_log(Level, const std::string& message) override {
                std::cout<<"sync: "<<message<<std::endl;
            }
        };
        auto logger = std::make_unique<SyncLogger>();
        logger->set_level_threshold(level);
        return std::move(logger);
    }
public:
    explicit App(const std::string& app_id, const std::optional<std::string>& base_url = {})
    {
        #if QT_CORE_LIB
        util::Scheduler::set_default_factory(util::make_qt);
        #endif
        SyncClientConfig config;
        bool should_encrypt = !getenv("REALM_DISABLE_METADATA_ENCRYPTION");
        config.logger_factory = defaultSyncLogger;
        #if REALM_DISABLE_METADATA_ENCRYPTION
        config.metadata_mode = SyncManager::MetadataMode::NoEncryption;
        #else
        config.metadata_mode = SyncManager::MetadataMode::Encryption;
        #endif
        #ifdef QT_CORE_LIB
        auto qt_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        if (!std::filesystem::exists(qt_path)) {
            std::filesystem::create_directory(qt_path);
        }
        config.base_file_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        #else
        config.base_file_path = std::filesystem::current_path();
        #endif
        config.user_agent_binding_info = "RealmCpp/0.0.1";
        config.user_agent_application_info = app_id;

        m_app = app::App::get_shared_app(app::App::Config{
            .app_id=app_id,
            .transport = std::make_shared<internal::DefaultTransport>(),
            .base_url = base_url ? base_url : util::Optional<std::string>(),
            .platform="Realm Cpp",
            .platform_version="?",
            .sdk_version="0.0.1",
        }, config);
    }

    struct credentials {
        static credentials anonymous()
        {
            return credentials(app::AppCredentials::anonymous());
        }
        static credentials api_key(const std::string& key)
        {
            return credentials(app::AppCredentials::user_api_key(key));
        }
        static credentials facebook(const std::string& access_token)
        {
            return credentials(app::AppCredentials::facebook(access_token));
        }
        static credentials apple(const std::string& id_token)
        {
            return credentials(app::AppCredentials::apple(id_token));
        }
        static credentials google(app::AuthCode auth_code)
        {
            return credentials(app::AppCredentials::google(std::move(auth_code)));
        }
        static credentials google(app::IdToken id_token)
        {
            return credentials(app::AppCredentials::google(std::move(id_token)));
        }
        static credentials custom(const std::string& token)
        {
            return credentials(app::AppCredentials::custom(token));
        }
        static credentials username_password(const std::string& username, const std::string& password)
        {
            return credentials(app::AppCredentials::username_password(username, password));
        }
        static credentials function(const bson::BsonDocument& payload)
        {
            return credentials(app::AppCredentials::function(payload));
        }
        credentials() = delete;
        credentials(const credentials& credentials) = default;
        credentials(credentials&&) = default;
    private:
        explicit credentials(app::AppCredentials&& credentials)
        : m_credentials(credentials)
        {
        }
        friend class App;
        app::AppCredentials m_credentials;
    };

    task<void> register_user(const std::string username, const std::string password) {
        auto error = co_await make_awaitable<util::Optional<app::AppError>>([&](auto cb) {
            m_app->template provider_client<app::App::UsernamePasswordProviderClient>().register_email(username,
                                                                                                       password,
                                                                                                       cb);
        });
        if (error) {
            throw *error;
        }
        co_return;
    }

    task<user> login(const credentials& credentials) {
        try {
            auto u = co_await make_awaitable<std::shared_ptr<SyncUser>>([this, credentials = std::move(credentials)](auto cb) {
                m_app->log_in_with_credentials(credentials.m_credentials, cb);
            });
            co_return std::move(user{std::move(u), m_app});
        } catch (std::exception& err) {
            throw;
        }
    }

    void login(const credentials& credentials, util::UniqueFunction<void(user, std::optional<app_error>)>&& callback) {
        m_app->log_in_with_credentials(credentials.m_credentials, [cb = std::move(callback), this](auto& u, auto error) {
            cb(user{std::move(u), m_app}, error ? std::optional<app_error>{app_error(*error)} : std::nullopt);
        });
    }

private:
    std::shared_ptr<app::App> m_app;
};

}
#endif /* Header_h */
