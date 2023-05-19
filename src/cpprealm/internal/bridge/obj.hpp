#ifndef REALM_INTERNAL_OBJ_HPP
#define REALM_INTERNAL_OBJ_HPP

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <cpprealm/internal/bridge/binary.hpp>
#include <cpprealm/internal/bridge/col_key.hpp>
#include <cpprealm/internal/bridge/dictionary.hpp>
#include <cpprealm/internal/bridge/object.hpp>
#include <cpprealm/internal/bridge/object_id.hpp>

namespace realm {
    class Group;
    class Realm;
    class Timestamp;
    class Object;
    class ObjectSchema;
    class Obj;
    class TableRef;
    class Query;
    struct ColKey;
    class LnkLst;
    template <typename T>
    struct object_base;
    struct NotificationToken;

    namespace internal::bridge {
        struct obj_key;
        struct obj_link;
    }
    namespace internal::type_info {
        template <typename T, std::enable_if_t<std::is_base_of_v<object_base<T>, T>>>
        bridge::obj_key serialize(const T& o);
        template <typename, typename>
        struct type_info;
    }

    template <typename, typename>
    struct persisted;
}

namespace realm::internal::bridge {
    struct obj;
    struct mixed;
    struct realm;
    struct lnklst;
    struct query;
    struct object_schema;
    struct timestamp;
    struct obj_key;
    struct obj_link;
    struct table;
    struct dictionary;
    struct uuid;
    struct object_id;
    struct list;
    struct row;

    namespace {
        template <typename T>
        struct is_optional : std::false_type {
            using underlying = T;
        };

        template <typename T>
        struct is_optional<std::optional<T>> : std::true_type {
            using underlying = T;
        };
    }

    template <typename T>
    [[nodiscard]] T get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] std::string get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] uuid get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] object_id get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] binary get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] timestamp get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] int64_t get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] double get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] bool get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] mixed get(const obj&, const col_key& col_key);
    template <>
    [[nodiscard]] core_dictionary get(const obj&, const col_key& col_key);

    struct obj {
        obj();
        obj(const obj& other) ;
        obj& operator=(const obj& other) ;
        obj(obj&& other);
        obj& operator=(obj&& other);
        ~obj();
        obj(const Obj&); //NOLINT google-explicit-constructor
        operator Obj() const; //NOLINT google-explicit-constructor
        [[nodiscard]] table get_table() const noexcept;
        [[nodiscard]] table get_target_table(col_key) const noexcept;
        [[nodiscard]] bool is_null(const col_key& col_key) const;
        [[nodiscard]] bool is_valid() const;
        obj get_linked_object(const col_key& col_key);
        template <typename T>
        T get(const col_key& col_key) const {
            if constexpr (is_optional<T>::value) {
                if (is_null(col_key)) {
                    return std::nullopt;
                }
                return
                    persisted<typename T::value_type, void>::deserialize
                            (internal::bridge::get<typename type_info::type_info<typename T::value_type, void>::internal_type>(*this, col_key));
            } else {
                return internal::bridge::get<T>(*this, col_key);
            }
        }

        template <typename T>
        std::optional<T> get_optional(const col_key& col_key) const {
            if (is_null(col_key)) {
                return std::nullopt;
            }
            return internal::bridge::get<T>(*this, col_key);
        }

        void set(const col_key& col_key, const int64_t& value);
        void set(const col_key& col_key, const double& value);
        void set(const col_key& col_key, const std::string& value);
        void set(const col_key& col_key, const mixed& value);
        void set(const col_key& col_key, const bool& value);
        void set(const col_key& col_key, const timestamp& value);
        void set(const col_key& col_key, const binary& value);
        void set(const col_key& col_key, const uuid& value);
        void set(const col_key& col_key, const object_id& value);
        void set(const col_key& col_key, const obj_key& value);
        void set(const col_key& col_key, const std::chrono::time_point<std::chrono::system_clock>& value);
        template<typename T>
        void set(const col_key& col_key, const std::optional<T>& value) {
            if (value) {
                set(col_key, *value);
            } else {
                set_null(col_key);
            }
        }

        void set_list_values(const col_key& col_key, const std::vector<obj_key>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::string>& values);
        void set_list_values(const col_key& col_key, const std::vector<bool>& values);
        void set_list_values(const col_key& col_key, const std::vector<int64_t>& values);
        void set_list_values(const col_key& col_key, const std::vector<double>& values);
        void set_list_values(const col_key& col_key, const std::vector<internal::bridge::uuid>& values);
        void set_list_values(const col_key& col_key, const std::vector<internal::bridge::object_id>& values);
        void set_list_values(const col_key& col_key, const std::vector<binary>& values);
        void set_list_values(const col_key& col_key, const std::vector<mixed>& values);
        void set_list_values(const col_key& col_key, const std::vector<timestamp>& values);

        void set_list_values(const col_key& col_key, const std::vector<std::optional<int64_t>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<bool>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<double>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<std::string>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<obj_key>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<internal::bridge::uuid>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<internal::bridge::object_id>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<binary>>& values);
        void set_list_values(const col_key& col_key, const std::vector<std::optional<timestamp>>& values);

        template <typename ValueType>
        void set_list_values(const col_key& col_key, const std::vector<ValueType>& values) {
            std::vector<typename internal::type_info::type_info<ValueType, void>::internal_type> v2;
            for (auto v : values) {
                v2.push_back(persisted<ValueType, void>::serialize(v));
            }
            set_list_values(col_key, v2);
        }

        [[nodiscard]] obj_key get_key() const;
        [[nodiscard]] obj_link get_link() const;
        lnklst get_linklist(const col_key& col_key);
        core_dictionary get_dictionary(const col_key& col_key);
        void set_null(const col_key&);
        obj create_and_set_linked_object(const col_key&);
    private:
        template <typename T>
        friend T get(const obj&, const col_key& col_key);
#ifdef __i386__
        std::aligned_storage<44, 4>::type m_obj[1];
#elif __x86_64__
        std::aligned_storage<64, 8>::type m_obj[1];
#elif __arm__
        std::aligned_storage<56, 8>::type m_obj[1];
#elif __aarch64__
        std::aligned_storage<64, 8>::type m_obj[1];
#elif _WIN32
        std::aligned_storage<64, 8>::type m_obj[1];
#endif
    };

    struct group {
        group(realm&);
        table get_table(uint32_t table_key);
        table get_table(const std::string& table_key);
    private:
        std::reference_wrapper<realm> m_realm;
    };

    std::string table_name_for_object_type(const std::string&);
}


#endif //REALM_INTERNAL_OBJ_HPP
