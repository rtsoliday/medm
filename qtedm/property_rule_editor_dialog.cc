#include "property_rule_editor_dialog.h"

#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

PropertyRuleEditorDialog::PropertyRuleEditorDialog(
    const QtedmRuleSet &ruleSet, QWidget *parent)
  : QDialog(parent)
  , parsedRuleSet_(ruleSet)
{
  setWindowTitle(QStringLiteral("Declarative Property Rules"));
  resize(760, 520);

  auto *layout = new QVBoxLayout(this);
  auto *help = new QLabel(QStringLiteral(
      "Rules use the MEDM calculation grammar and typed A-L PV inputs. "
      "Only visibility, enabled, text, foreground, background, and geometry "
      "may be changed. No I/O or PV writes are available."));
  help->setWordWrap(true);
  layout->addWidget(help);

  editor_ = new QPlainTextEdit;
  editor_->setObjectName(QStringLiteral("qtedmRuleJsonEditor"));
  editor_->setPlainText(QString::fromUtf8(QJsonDocument(
      PropertyRules::toJson(ruleSet)).toJson(QJsonDocument::Indented)));
  layout->addWidget(editor_, 1);

  diagnosticLabel_ = new QLabel;
  diagnosticLabel_->setObjectName(QStringLiteral("qtedmRuleDiagnostics"));
  diagnosticLabel_->setWordWrap(true);
  layout->addWidget(diagnosticLabel_);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  auto *validateButton = buttons->addButton(QStringLiteral("Validate"),
      QDialogButtonBox::ActionRole);
  QObject::connect(validateButton, &QPushButton::clicked, this, [this]() {
    QString error;
    if (validateEditor(&error)) {
      diagnosticLabel_->setText(QStringLiteral("Rules are valid."));
      diagnosticLabel_->setStyleSheet(QStringLiteral("color: #006400;"));
    } else {
      diagnosticLabel_->setText(error);
      diagnosticLabel_->setStyleSheet(QStringLiteral("color: #8b0000;"));
    }
  });
  QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    QString error;
    if (validateEditor(&error)) {
      accept();
    } else {
      diagnosticLabel_->setText(error);
      diagnosticLabel_->setStyleSheet(QStringLiteral("color: #8b0000;"));
    }
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected,
      this, &QDialog::reject);
  layout->addWidget(buttons);
}

QtedmRuleSet PropertyRuleEditorDialog::ruleSet() const
{
  return parsedRuleSet_;
}

bool PropertyRuleEditorDialog::validateEditor(QString *error)
{
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(
      editor_->toPlainText().toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    if (error) {
      *error = QStringLiteral("JSON parse error at offset %1: %2")
          .arg(parseError.offset).arg(parseError.errorString());
    }
    return false;
  }
  QtedmRuleSet parsed;
  if (!PropertyRules::fromJson(document.array(), &parsed, error)) {
    return false;
  }
  parsedRuleSet_ = parsed;
  return true;
}

