#ifndef CPP_REALM_BRIDGE_OBJECT_ID_HPP
#define CPP_REALM_BRIDGE_OBJECT_ID_HPP

#include <cpprealm/internal/bridge/utils.hpp>
#include <array>

namespace realm {
    struct object_id;
    class ObjectId;
}

namespace realm::internal::bridge {
    struct object_id : core_binding<ObjectId> {
        object_id();
        object_id(const object_id& other) = default;
        object_id& operator=(const object_id& other) = default;
        object_id(object_id&& other) = default;
        object_id& operator=(object_id&& other) = default;
        ~object_id() = default;
        object_id(const ObjectId&); //NOLINT(google-explicit-constructor);
        explicit object_id(const std::string&);
        object_id(const struct ::realm::object_id&); //NOLINT(google-explicit-constructor);
        operator ObjectId() const final; //NOLINT(google-explicit-constructor);
        operator ::realm::object_id() const; //NOLINT(google-explicit-constructor);
        [[nodiscard]] std::string to_string() const;
        [[nodiscard]] static object_id generate();
    private:
        std::array<uint8_t, 12> m_object_id;

        friend bool operator ==(const object_id&, const object_id&);
        friend bool operator !=(const object_id&, const object_id&);
        friend bool operator >(const object_id&, const object_id&);
        friend bool operator <(const object_id&, const object_id&);
        friend bool operator >=(const object_id&, const object_id&);
        friend bool operator <=(const object_id&, const object_id&);
    };

    bool operator ==(const object_id&, const object_id&);
    bool operator !=(const object_id&, const object_id&);
    bool operator >(const object_id&, const object_id&);
    bool operator <(const object_id&, const object_id&);
    bool operator >=(const object_id&, const object_id&);
    bool operator <=(const object_id&, const object_id&);
}

#endif //CPP_REALM_BRIDGE_OBJECT_ID_HPP

