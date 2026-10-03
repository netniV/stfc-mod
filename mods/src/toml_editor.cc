#include "toml_editor.h"
#include "config_save.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace config_edit
{
namespace
{
  std::string Encode(const Value& value)
  {
    return std::visit(
        [](const auto& item) {
          const toml::value  node(item);
          std::ostringstream output;
          output.exceptions(std::ios::badbit | std::ios::failbit);
          output << toml::toml_formatter(node, toml::format_flags::none);
          return output.str();
        },
        value);
  }

  std::optional<Value> ReadValue(const toml::node* node)
  {
    if (!node)
      return std::nullopt;
    if (node->is_boolean())
      return Value{node->as_boolean()->get()};
    if (node->is_string())
      return Value{node->as_string()->get()};
    if (node->is_floating_point())
      return Value{node->as_floating_point()->get()};
    if (node->is_integer())
      return Value{node->as_integer()->get()};
    throw std::invalid_argument("unsupported setting type");
  }

  std::size_t BomSize(std::string_view text)
  { return text.starts_with("\xef\xbb\xbf") ? 3 : 0; }

  // toml++ columns count Unicode codepoints, including CR; only LF starts a line.
  std::size_t Offset(std::string_view text, toml::source_position target)
  {
    toml::source_position current{1, 1};
    for (auto i = BomSize(text);;) {
      if (current == target)
        return i;
      if (i >= text.size())
        throw std::invalid_argument("invalid source region");
      const auto byte = static_cast<unsigned char>(text[i]);
      if (byte == '\n') {
        ++current.line;
        current.column = 1;
      } else
        ++current.column;
      ++i;
      // Input has already passed TOML/UTF-8 validation.
      while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xc0) == 0x80)
        ++i;
    }
  }
} // namespace

Prepared TomlEditor::Prepare(const std::string& text, const Request& request)
{
  try {
    if (const auto* value = std::get_if<double>(&request.desired); value && !std::isfinite(*value))
      return {Outcome::Unsupported, {}};
    if (!cached_table_ || cached_text_ != text) {
      auto parsed = toml::parse(text);
      cached_table_.reset(); // Never pair new bytes with stale regions if allocation fails.
      cached_text_  = text;
      cached_table_ = std::move(parsed);
    }
    const auto& document = *cached_table_;
    const auto* parent   = document.get_as<toml::table>(request.section);
    if (document.contains(request.section) && !parent)
      return {Outcome::Unsupported, {}};
    const auto* node    = parent ? parent->get(request.key) : nullptr;
    const auto  current = ReadValue(node);
    if (current && *current == request.desired)
      return {Outcome::AlreadySaved, {}};
    if (current != request.expected)
      return {Outcome::Conflict, {}};

    auto desired_document = document;
    if (!parent)
      desired_document.insert(request.section, toml::table{});
    std::visit(
        [&](const auto& value) {
          desired_document.get_as<toml::table>(request.section)->insert_or_assign(request.key, value);
        },
        request.desired);

    // Accept a candidate only if a fresh parse has exactly the intended meaning.
    auto accepts = [&](const std::string& candidate) {
      try {
        return toml::parse(candidate) == desired_document;
      } catch (const toml::parse_error&) {
        return false;
      }
    };
    const auto encoded = Encode(request.desired);
    if (node) {
      const auto begin = Offset(text, node->source().begin);
      const auto end   = Offset(text, node->source().end);
      if (end < begin || end > text.size())
        return {Outcome::Unsupported, {}};
      auto result = text;
      result.replace(begin, end - begin, encoded);
      if (accepts(result))
        return {Outcome::Prepared, std::move(result)};
      return {Outcome::Unsupported, {}};
    }

    const auto newline    = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    const auto assignment = Encode(Value{request.key}) + " = " + encoded;
    if (parent && parent->is_inline()) {
      const auto end = Offset(text, parent->source().end);
      if (!end || text[end - 1] != '}')
        return {Outcome::Unsupported, {}};
      auto result = text;
      result.insert(end - 1, (parent->empty() ? " " : ", ") + assignment + " ");
      if (accepts(result))
        return {Outcome::Prepared, std::move(result)};
      return {Outcome::Unsupported, {}};
    }
    if (parent) {
      const auto begin = Offset(text, parent->source().begin);
      if (begin < text.size() && text[begin] == '[') {
        auto end    = text.find('\n', begin);
        auto result = text;
        if (end == std::string::npos)
          result += std::string(newline) + assignment + newline;
        else
          result.insert(end + 1, assignment + newline);
        if (accepts(result))
          return {Outcome::Prepared, std::move(result)};
      }
      // Dotted/implicit tables can sometimes be extended at the document root.
      auto result = text;
      result.insert(BomSize(text), Encode(Value{request.section}) + "." + assignment + newline);
      if (accepts(result))
        return {Outcome::Prepared, std::move(result)};
    }
    auto result = text;
    if (!result.empty() && result.back() != '\n')
      result += newline;
    result += "[" + Encode(Value{request.section}) + "]" + newline + assignment + newline;
    if (accepts(result))
      return {Outcome::Prepared, std::move(result)};
    return {Outcome::Unsupported, {}};
  } catch (const toml::parse_error&) {
    return {Outcome::InvalidDocument, {}};
  } catch (const std::invalid_argument&) {
    return {Outcome::Unsupported, {}};
  }
}

Outcome TomlEditor::Save(const std::filesystem::path& path, const Request& request)
{
  try {
    const auto original = ReadConfigText(path);
    auto       edit     = Prepare(original, request);
    if (edit.outcome != Outcome::Prepared)
      return edit.outcome;
    if (!ReplaceConfigText(path, edit.text, original))
      return Outcome::Conflict;
    // Source locations belong to the old document; refresh on the next request.
    cached_table_.reset();
    cached_text_.clear();
    return Outcome::Saved;
  } catch (const std::exception&) {
    return Outcome::IoError;
  }
}
} // namespace config_edit
