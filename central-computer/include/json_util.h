#ifndef CENTRAL_COMPUTER_JSON_UTIL_H
#define CENTRAL_COMPUTER_JSON_UTIL_H

#include <sstream>
#include <string>
#include <vector>

namespace submarine::json {

// Deliberately minimal, hand-written, write-only JSON helpers - this
// project has no network access to fetch a JSON library, and the
// dashboard API only ever *emits* JSON (it never needs to parse a JSON
// request body - see http_server.h's comment on why POST bodies are
// simple form-encoded key=value pairs instead). Everything here just
// builds a std::string; there is no DOM/value type to keep this small.

// Escapes a string for safe embedding inside a JSON string literal.
inline std::string escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

inline std::string str(const std::string& s) { return "\"" + escape(s) + "\""; }

template <typename T>
std::string num(T value) {
  std::ostringstream ss;
  ss << value;
  return ss.str();
}

inline std::string boolean(bool b) { return b ? "true" : "false"; }

// A tiny fluent object/array builder - not a general JSON value type, just
// enough to assemble the fixed-shape responses dashboard_api.cpp needs.
class ObjectBuilder {
 public:
  ObjectBuilder& field(const std::string& key, const std::string& rawJsonValue) {
    if (!first_) out_ += ",";
    first_ = false;
    out_ += str(key) + ":" + rawJsonValue;
    return *this;
  }
  ObjectBuilder& strField(const std::string& key, const std::string& value) {
    return field(key, str(value));
  }
  template <typename T>
  ObjectBuilder& numField(const std::string& key, T value) {
    return field(key, num(value));
  }
  ObjectBuilder& boolField(const std::string& key, bool value) { return field(key, boolean(value)); }

  std::string build() const { return "{" + out_ + "}"; }

 private:
  std::string out_;
  bool first_ = true;
};

inline std::string array(const std::vector<std::string>& items) {
  std::string out = "[";
  for (size_t i = 0; i < items.size(); i++) {
    if (i) out += ",";
    out += items[i];
  }
  out += "]";
  return out;
}

}  // namespace submarine::json

#endif  // CENTRAL_COMPUTER_JSON_UTIL_H
