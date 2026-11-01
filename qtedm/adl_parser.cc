#include "adl_parser.h"



#include <QChar>
#include <QRegularExpression>



#include <algorithm>
#include <cctype>



namespace {

QString macroSizeErrorMessage()
{
  return QStringLiteral(
      "Macro expansion exceeds maximum size of %1 bytes or %2 characters")
      .arg(kMaximumAdlInputBytes).arg(kMaximumAdlInputCharacters);
}

/* Counts UTF-8 bytes without allocating a potentially oversized copy. */
qint64 utf8Length(const QString &text)
{
  qint64 bytes = 0;
  for (qsizetype index = 0; index < text.size(); ++index) {
    const ushort ch = text.at(index).unicode();
    if (ch < 0x80) {
      ++bytes;
    } else if (ch < 0x800) {
      bytes += 2;
    } else if (QChar::isHighSurrogate(ch) && index + 1 < text.size()
        && text.at(index + 1).isLowSurrogate()) {
      bytes += 4;
      ++index;
    } else {
      bytes += 3;
    }
  }
  return bytes;
}

qsizetype longestMacroName(const QHash<QString, QString> &macros)
{
  qsizetype longest = 0;
  for (auto it = macros.cbegin(); it != macros.cend(); ++it) {
    longest = std::max<qsizetype>(longest, it.key().size());
  }
  return longest;
}

/* True when text has a $(name) reference at index. Names longer than
 * maximumNameLength cannot be defined, so the search for ')' stops there
 * instead of scanning the rest of a long line. */
bool macroReferenceAt(const QString &text, qsizetype index,
    qsizetype maximumNameLength, QString *name, qsizetype *end)
{
  if (index + 1 >= text.size() || text.at(index) != QLatin1Char('$')
      || text.at(index + 1) != QLatin1Char('(')) {
    return false;
  }
  const qsizetype nameStart = index + 2;
  const qsizetype limit = std::min<qsizetype>(text.size(),
      nameStart + maximumNameLength + 1);
  for (qsizetype close = nameStart; close < limit; ++close) {
    if (text.at(close) == QLatin1Char(')')) {
      if (close == nameStart) {
        return false;
      }
      *name = text.mid(nameStart, close - nameStart);
      *end = close + 1;
      return true;
    }
  }
  return false;
}

struct ExpandedMacro
{
  QString value;
  qint64 bytes = 0;
};

/* Expands each macro once, on first use, so unused macros cost nothing. */
class MacroExpansionCache
{
public:
  explicit MacroExpansionCache(const QHash<QString, QString> &macros)
    : macros_(macros)
    , longestName_(longestMacroName(macros))
  {
  }

  bool isEmpty() const
  {
    return macros_.isEmpty();
  }

  qsizetype longestName() const
  {
    return longestName_;
  }

  /* Returns false when name is not defined. Otherwise sets error when the
   * value cannot be expanded within the size limits. */
  bool lookup(const QString &name, ExpandedMacro *result, QString *error)
  {
    const auto cached = expanded_.constFind(name);
    if (cached != expanded_.constEnd()) {
      *result = *cached;
      return true;
    }
    const auto raw = macros_.constFind(name);
    if (raw == macros_.constEnd()) {
      return false;
    }
    ExpandedMacro expanded;
    if (!expandMacros(*raw, macros_, &expanded.value, error)) {
      return true;
    }
    expanded.bytes = utf8Length(expanded.value);
    expanded_.insert(name, expanded);
    *result = expanded;
    return true;
  }

private:
  const QHash<QString, QString> &macros_;
  qsizetype longestName_ = 0;
  QHash<QString, ExpandedMacro> expanded_;
};



class Parser

{

public:

  Parser(const QString &text, QString *errorMessage,
      int maximumNestingDepth, const QHash<QString, QString> &macros)

    : text_(text)

    , errorMessage_(errorMessage)

    , maximumNestingDepth_(maximumNestingDepth)

    , macros_(macros)

    , outputCharacters_(text.size())

  {

  }



  std::optional<AdlNode> parse()

  {

    skipWhitespace();

    AdlNode root;

    root.name = QStringLiteral("root");

    while (!atEnd()) {

      /* As MEDM's display list parser does, skip a closing brace that
       * matches no block, such as a stray one between two objects. */

      if (peek() == QChar('}')) {

        ++index_;

        skipWhitespace();

        continue;

      }

      std::optional<AdlNode> child = parseNode(0);

      if (!child) {

        return std::nullopt;

      }

      root.children.append(std::move(*child));

      skipWhitespace();

    }

    return root;

  }



private:

  bool atEnd() const

  {

    return index_ >= text_.size();

  }



  QChar peek() const

  {

    if (atEnd()) {

      return QChar();

    }

    return text_.at(index_);

  }



  QChar get()

  {

    if (atEnd()) {

      return QChar();

    }

    return text_.at(index_++);

  }



  void skipWhitespace()

  {

    while (!atEnd()) {

      const QChar ch = peek();

      if (ch.isSpace()) {

        ++index_;

        continue;

      }

      if (ch == QChar('#')) {

        while (!atEnd() && peek() != QChar('\n')) {

          ++index_;

        }

        continue;

      }

      break;

    }

  }



  /* Appends the value of a defined macro referenced at index_, as MEDM's
   * tokenizer does, and records where it went. Returns true when it
   * consumed a reference or set a size error. */
  bool appendMacroValue(QString *result, QList<AdlMacroUse> *uses)
  {
    if (macros_.isEmpty()) {
      return false;
    }
    QString name;
    qsizetype end = 0;
    if (!macroReferenceAt(text_, index_, macros_.longestName(), &name,
            &end)) {
      return false;
    }
    ExpandedMacro macro;
    QString error;
    if (!macros_.lookup(name, &macro, &error)) {
      return false;
    }
    if (!error.isEmpty()) {
      setError(error);
      return true;
    }
    const qsizetype referenceLength = end - index_;
    if (outputBytes_ < 0) {
      outputBytes_ = utf8Length(text_);
    }
    if (macro.value.size() - referenceLength
            > kMaximumAdlInputCharacters - outputCharacters_
        || macro.bytes - referenceLength
            > kMaximumAdlInputBytes - outputBytes_) {
      setError(macroSizeErrorMessage());
      return true;
    }
    outputCharacters_ += macro.value.size() - referenceLength;
    outputBytes_ += macro.bytes - referenceLength;
    if (uses && !macro.value.isEmpty()) {
      uses->append({result->size(), name, macro.value});
    }
    result->append(macro.value);
    index_ = static_cast<int>(end);
    return true;
  }



  std::optional<QString> parseQuotedString(QList<AdlMacroUse> *uses)

  {

    if (get() != QChar('"')) {

      return std::nullopt;

    }

    QString result;

    while (!atEnd()) {

      if (peek() == QChar('$') && appendMacroValue(&result, uses)) {
        if (parseError_) {
          return std::nullopt;
        }
        continue;
      }

      QChar ch = get();

      if (ch == QChar('"')) {

        return result;

      }

      if (ch == QChar('\\')) {

        if (atEnd()) {

          result.append(ch);

          break;

        }

        const QChar escaped = get();
        switch (escaped.unicode()) {
        case '\\':
          result.append(QChar('\\'));
          break;
        case '"': {
          /* MEDM allows a literal backslash before the closing quote.
           * QtEDM also writes escaped quotes, so retain that interpretation
           * when a closing quote remains on this line. Its writer escapes
           * newlines; do not consume subsequent ADL lines to find a quote. */
          bool closingQuote = false;
          for (int next = index_; next < text_.size(); ++next) {
            const QChar candidate = text_.at(next);
            if (candidate == QChar('\n') || candidate == QChar('\r')) {
              break;
            }
            if (candidate == QChar('\\')) {
              if (next + 1 < text_.size()
                  && text_.at(next + 1) != QChar('\n')
                  && text_.at(next + 1) != QChar('\r')) {
                ++next;
              }
            } else if (candidate == QChar('"')) {
              closingQuote = true;
              break;
            }
          }
          if (!closingQuote) {
            result.append(QChar('\\'));
            return result;
          }
          result.append(QChar('"'));
          break;
        }
        case '/':
          /* MEDM uses literal backslash-slash text for down arrowheads. */
          result.append(QChar('\\'));
          result.append(escaped);
          break;
        case 'n':
          result.append(QChar('\n'));
          break;
        case 'r':
          result.append(QChar('\r'));
          break;
        case 't':
          result.append(QChar('\t'));
          break;
        case 'u': {
          /* The writer escapes non-Latin-1 UTF-16 code units as exactly
           * four hex digits, including each half of a surrogate pair.
           * Preserve the existing unknown-escape behavior for malformed
           * or incomplete sequences without consuming their suffix. */
          bool valid = text_.size() - index_ >= 4;
          for (int digit = 0; valid && digit < 4; ++digit) {
            const QChar hex = text_.at(index_ + digit).toLower();
            valid = (hex >= QChar('0') && hex <= QChar('9'))
                || (hex >= QChar('a') && hex <= QChar('f'));
          }
          if (valid) {
            result.append(QChar(text_.mid(index_, 4).toUShort(nullptr, 16)));
            index_ += 4;
          } else {
            result.append(QChar('\\'));
            result.append(escaped);
          }
          break;
        }
        case 'x':
        case 'X': {
          QString digits;
          for (int i = 0; i < 2 && !atEnd(); ++i) {
            const QChar hex = peek();
            if (!hex.isDigit()
                && (hex.toLower() < QChar('a') || hex.toLower() > QChar('f'))) {
              break;
            }
            digits.append(get());
          }
          bool ok = false;
          const ushort value = digits.toUShort(&ok, 16);
          if (ok && !digits.isEmpty()) {
            result.append(QChar(value));
          } else {
            result.append(QChar('\\'));
            result.append(escaped);
            result.append(digits);
          }
          break;
        }
        default:
          /* MEDM reads backslashes literally, as in "Faults\Trips"; QtEDM
           * writes only the escapes handled above. */
          result.append(QChar('\\'));
          result.append(escaped);
          break;
        }
        continue;
      }

      result.append(ch);

    }

    setError(QStringLiteral("Unterminated string literal"));

    return std::nullopt;

  }



  QString parseToken(QList<AdlMacroUse> *uses = nullptr)

  {

    if (peek() == QChar('"')) {

      std::optional<QString> quoted = parseQuotedString(uses);

      return quoted.value_or(QString());

    }



    QString token;

    while (!atEnd()) {

      const QChar ch = peek();

      if (ch == QChar('$') && appendMacroValue(&token, uses)) {
        if (parseError_) {
          return QString();
        }
        continue;
      }

      if (ch.isSpace() || ch == QChar('{') || ch == QChar('}')

          || ch == QChar('=') || ch == QChar(',')) {

        break;

      }

      token.append(ch);

      ++index_;

    }

    return token;

  }



  std::optional<QString> parseValue(QList<AdlMacroUse> *uses)

  {

    skipWhitespace();

    const int valueStart = index_;

    QString value = parseToken(uses);

    if (parseError_) {

      return std::nullopt;

    }

    if (value.isNull() && index_ == valueStart) {

      setError(QStringLiteral("Expected value"));

      return std::nullopt;

    }

    skipWhitespace();

    if (peek() == QChar(',')) {

      ++index_;

    }

    return value;

  }



  std::optional<AdlNode> parseNode(int depth)

  {

    skipWhitespace();

    if (atEnd()) {

      return std::nullopt;

    }

    QList<AdlMacroUse> nameUses;

    QString name = parseToken(&nameUses);

    if (parseError_) {

      return std::nullopt;

    }

    if (name.isEmpty() && peek() != QChar('{')) {

      setError(QStringLiteral("Expected section or key"));

      return std::nullopt;

    }



    skipWhitespace();

    const QChar next = peek();

    if (next == QChar('=')) {

      ++index_;

      QList<AdlMacroUse> valueUses;

      std::optional<QString> value = parseValue(&valueUses);

      if (!value) {

        return std::nullopt;

      }

      AdlNode node;

      /*
       * Keep parser-internal assignments distinct from a real block named
       * "property".  QtEDM plugin ADL intentionally uses property { ... }
       * children, which must survive as nodes instead of being flattened.
       */
      node.name = QStringLiteral("__adl_assignment__");

      node.properties.append({name, *value, valueUses});

      return node;

    }



    if (next != QChar('{')) {

      AdlNode node;

      node.name = QStringLiteral("__adl_value__");

      node.properties.append({QString(), name, nameUses});

      return node;

    }

    if (depth >= maximumNestingDepth_) {

      setError(QStringLiteral("ADL nesting depth exceeds maximum of %1")

          .arg(maximumNestingDepth_));

      return std::nullopt;

    }



    ++index_; // consume '{'

    AdlNode node;

    node.name = name;

    while (true) {

      skipWhitespace();

      if (atEnd()) {

        setError(QStringLiteral("Unterminated block for %1").arg(name));

        return std::nullopt;

      }

      if (peek() == QChar(',')) {

        ++index_;

        continue;

      }

      if (peek() == QChar('}')) {

        ++index_;

        break;

      }

      std::optional<AdlNode> entry = parseNode(depth + 1);

      if (!entry) {

        return std::nullopt;

      }

      if (entry->name == QStringLiteral("__adl_assignment__")) {

        node.properties.append(entry->properties.first());

      } else if (entry->name == QStringLiteral("__adl_value__")) {

        node.properties.append(entry->properties.first());

      } else {

        node.children.append(std::move(*entry));

      }

    }

    return node;

  }



  void setError(const QString &message)

  {

    parseError_ = true;

    if (errorMessage_) {

      *errorMessage_ = message;

    }

  }



  QString text_;

  QString *errorMessage_ = nullptr;

  int maximumNestingDepth_ = kMaximumAdlNestingDepth;

  int index_ = 0;

  bool parseError_ = false;

  MacroExpansionCache macros_;

  /* Size of the text with macros substituted so far; bytes are counted on
   * the first substitution. */
  qsizetype outputCharacters_ = 0;

  qint64 outputBytes_ = -1;

};



} // namespace



std::optional<AdlNode> AdlParser::parse(const QString &text,
    QString *errorMessage, const QHash<QString, QString> &macros)
{
  if (text.size() > kMaximumAdlInputCharacters) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "ADL input exceeds maximum size of %1 characters")
          .arg(kMaximumAdlInputCharacters);
    }
    return std::nullopt;
  }
  Parser parser(text, errorMessage, kMaximumAdlNestingDepth, macros);
  return parser.parse();
}

bool expandMacros(const QString &input, const QHash<QString, QString> &macros,
    QString *output, QString *errorMessage)
{
  if (!output) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Macro expansion requires an output string");
    }
    return false;
  }
  output->clear();
  auto sizeError = [errorMessage]() {
    if (errorMessage) {
      *errorMessage = macroSizeErrorMessage();
    }
    return false;
  };
  if (input.size() > kMaximumAdlInputCharacters) {
    return sizeError();
  }
  if (macros.isEmpty()) {
    *output = input;
    return true;
  }

  static constexpr int kMaxIterations = 10;
  QRegularExpression pattern(QStringLiteral(R"(\$\(([^)]+)\))"));
  QString current = input;
  for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
    QString result;
    result.reserve(current.size());
    qint64 utf8Bytes = 0;
    auto appendBounded = [&result, &utf8Bytes](const QString &value,
        qsizetype start, qsizetype length) {
      if (length > kMaximumAdlInputCharacters - result.size()) {
        return false;
      }
      /* Count UTF-8 bytes without allocating a potentially oversized copy.
       * Macro/literal boundaries cannot split a valid surrogate pair. */
      qint64 bytes = 0;
      for (qsizetype index = start; index < start + length; ++index) {
        const ushort ch = value.at(index).unicode();
        if (ch < 0x80) {
          ++bytes;
        } else if (ch < 0x800) {
          bytes += 2;
        } else if (QChar::isHighSurrogate(ch) && index + 1 < start + length
            && value.at(index + 1).isLowSurrogate()) {
          bytes += 4;
          ++index;
        } else {
          bytes += 3;
        }
        if (bytes > kMaximumAdlInputBytes - utf8Bytes) {
          return false;
        }
      }
      result.append(value.constData() + start, length);
      utf8Bytes += bytes;
      return true;
    };
    int lastIndex = 0;
    QRegularExpressionMatchIterator it = pattern.globalMatch(current);
    bool replaced = false;
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      if (!appendBounded(current, lastIndex,
              match.capturedStart() - lastIndex)) {
        return sizeError();
      }
      const QString name = match.captured(1);
      const auto macroIt = macros.constFind(name);
      if (macroIt != macros.constEnd()) {
        if (!appendBounded(*macroIt, 0, macroIt->size())) {
          return sizeError();
        }
        replaced = true;
      } else {
        if (!appendBounded(current, match.capturedStart(),
                match.capturedLength())) {
          return sizeError();
        }
      }
      lastIndex = match.capturedEnd();
    }
    if (!appendBounded(current, lastIndex, current.size() - lastIndex)) {
      return sizeError();
    }
    if (!replaced) {
      *output = current;
      return true;
    }
    current = result;
  }
  *output = current;
  return true;
}

bool substituteMacroReferences(const QString &text,
    const QHash<QString, QString> &macros, QString *output,
    QList<AdlMacroUse> *uses, QString *errorMessage)
{
  if (!output || !uses) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "Macro substitution requires an output string and uses");
    }
    return false;
  }
  MacroExpansionCache cache(macros);
  const QList<AdlMacroUse> existing = *uses;
  QList<AdlMacroUse> updated;
  QString result;
  result.reserve(text.size());
  int nextExisting = 0;
  for (qsizetype index = 0; index < text.size();) {
    const qsizetype existingStart = nextExisting < existing.size()
        ? existing.at(nextExisting).position : text.size();
    if (index == existingStart) {
      AdlMacroUse use = existing.at(nextExisting++);
      const qsizetype length = use.value.size();
      use.position = result.size();
      result.append(text.mid(index, length));
      updated.append(use);
      index += length;
      continue;
    }
    QString name;
    qsizetype end = 0;
    ExpandedMacro macro;
    QString error;
    if (macroReferenceAt(text, index, cache.longestName(), &name, &end)
        && end <= existingStart && cache.lookup(name, &macro, &error)) {
      if (error.isEmpty()
          && macro.value.size() > kMaximumAdlInputCharacters - result.size()) {
        error = macroSizeErrorMessage();
      }
      if (!error.isEmpty()) {
        if (errorMessage) {
          *errorMessage = error;
        }
        return false;
      }
      if (!macro.value.isEmpty()) {
        updated.append({result.size(), name, macro.value});
      }
      result.append(macro.value);
      index = end;
      continue;
    }
    result.append(text.at(index++));
  }
  *output = result;
  *uses = updated;
  return true;
}

bool macroUsesMatch(const QString &text, const QList<AdlMacroUse> &uses)
{
  qsizetype previousEnd = 0;
  for (const AdlMacroUse &use : uses) {
    if (use.position < previousEnd || use.value.isEmpty()
        || use.value.size() > text.size() - use.position
        || QStringView(text).mid(use.position, use.value.size())
            != QStringView(use.value)) {
      return false;
    }
    previousEnd = use.position + use.value.size();
  }
  return true;
}

const AdlProperty *findProperty(const AdlNode &node, const QString &key)
{
  for (const auto &prop : node.properties) {
    if (prop.key.compare(key, Qt::CaseInsensitive) == 0) {
      return &prop;
    }
  }
  return nullptr;
}

QString propertyValue(const AdlNode &node, const QString &key,
    const QString &defaultValue)
{
  if (const AdlProperty *prop = findProperty(node, key)) {
    return prop->value;
  }
  return defaultValue;
}

QString normalizedAdlName(const QString &name)
{
  QString normalized = name.trimmed();
  if (normalized.startsWith(QLatin1String("<<")) &&
      normalized.endsWith(QLatin1String(">>")) &&
      normalized.size() > 4) {
    normalized = normalized.mid(2, normalized.size() - 4).trimmed();
  }
  return normalized.toLower();
}

const AdlNode *findChild(const AdlNode &node, const QString &name)
{
  const QString target = normalizedAdlName(name);
  for (const auto &child : node.children) {
    if (normalizedAdlName(child.name) == target) {
      return &child;
    }
  }
  return nullptr;
}

QList<const AdlNode *> findChildren(const AdlNode &node, const QString &name)
{
  const QString target = normalizedAdlName(name);
  QList<const AdlNode *> matches;
  for (const auto &child : node.children) {
    if (normalizedAdlName(child.name) == target) {
      matches.append(&child);
    }
  }
  return matches;
}
