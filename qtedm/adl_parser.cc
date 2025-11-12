#include "adl_parser.h"



#include <QChar>



#include <cctype>



namespace {



class Parser

{

public:

  Parser(const QString &text, QString *errorMessage)

    : text_(text)

    , errorMessage_(errorMessage)

  {

  }



  std::optional<AdlNode> parse()

  {

    skipWhitespace();

    AdlNode root;

    root.name = QStringLiteral("root");

    while (!atEnd()) {

      std::optional<AdlNode> child = parseNode();

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



  std::optional<QString> parseQuotedString()

  {

    if (get() != QChar('"')) {

      return std::nullopt;

    }

    QString result;

    while (!atEnd()) {

      QChar ch = get();

      if (ch == QChar('"')) {

        return result;

      }

      /* ADL files do not use escape sequences; all characters within quotes

       * are treated literally, matching medm's getToken() behavior. */

      result.append(ch);

    }

    setError(QStringLiteral("Unterminated string literal"));

    return std::nullopt;

  }



  QString parseToken()

  {

    if (peek() == QChar('"')) {

      std::optional<QString> quoted = parseQuotedString();

      return quoted.value_or(QString());

    }



    QString token;

    while (!atEnd()) {

      const QChar ch = peek();

      if (ch.isSpace() || ch == QChar('{') || ch == QChar('}')

          || ch == QChar('=') || ch == QChar(',')) {

        break;

      }

      token.append(ch);

      ++index_;

    }

    return token;

  }



  std::optional<QString> parseValue()

  {

    skipWhitespace();

    const int valueStart = index_;

    QString value = parseToken();

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



  std::optional<AdlNode> parseNode()

  {

    skipWhitespace();

    if (atEnd()) {

      return std::nullopt;

    }

    QString name = parseToken();

    if (name.isEmpty() && peek() != QChar('{')) {

      setError(QStringLiteral("Expected section or key"));

      return std::nullopt;

    }



    skipWhitespace();

    const QChar next = peek();

    if (next == QChar('=')) {

      ++index_;

      std::optional<QString> value = parseValue();

      if (!value) {

        return std::nullopt;

      }

      AdlNode node;

      node.name = QStringLiteral("property");

      node.properties.append({name, *value});

      return node;

    }



    if (next != QChar('{')) {

      AdlNode node;

      node.name = QStringLiteral("value");

      node.properties.append({QString(), name});

      return node;

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

      std::optional<AdlNode> entry = parseNode();

      if (!entry) {

        return std::nullopt;

      }

      if (entry->name == QStringLiteral("property")) {

        node.properties.append(entry->properties.first());

      } else if (entry->name == QStringLiteral("value")) {

        node.properties.append(entry->properties.first());

      } else {

        node.children.append(std::move(*entry));

      }

    }

    return node;

  }



  void setError(const QString &message)

  {

    if (errorMessage_) {

      *errorMessage_ = message;

    }

  }



  QString text_;

  QString *errorMessage_ = nullptr;

  int index_ = 0;

};



} // namespace



std::optional<AdlNode> AdlParser::parse(const QString &text,
    QString *errorMessage)
{
  Parser parser(text, errorMessage);
  return parser.parse();
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

const AdlNode *findChild(const AdlNode &node, const QString &name)
{
  for (const auto &child : node.children) {
    if (child.name.compare(name, Qt::CaseInsensitive) == 0) {
      return &child;
    }
  }
  return nullptr;
}

QList<const AdlNode *> findChildren(const AdlNode &node, const QString &name)
{
  QList<const AdlNode *> matches;
  for (const auto &child : node.children) {
    if (child.name.compare(name, Qt::CaseInsensitive) == 0) {
      matches.append(&child);
    }
  }
  return matches;
}
