#pragma once

#include <QDialog>

#include "property_rules.h"

class QLabel;
class QPlainTextEdit;

class PropertyRuleEditorDialog : public QDialog
{
public:
  explicit PropertyRuleEditorDialog(const QtedmRuleSet &ruleSet,
      QWidget *parent = nullptr);

  QtedmRuleSet ruleSet() const;
  bool validateEditor(QString *error = nullptr);

private:
  QPlainTextEdit *editor_ = nullptr;
  QLabel *diagnosticLabel_ = nullptr;
  QtedmRuleSet parsedRuleSet_;
};

