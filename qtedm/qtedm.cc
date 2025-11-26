#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPageSetupDialog>
#include <QPalette>
#include <QPointer>
#include <QPoint>
#include <QPrintDialog>
#include <QPrinter>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QSizePolicy>
#include <QStackedLayout>
#include <QStyleFactory>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QResource>

#include <climits>
#include <cstring>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#include <QAbstractNativeEventFilter>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <xcb/xcb.h>
#include "../medm/medmVersion.h"
#ifdef Status
#undef Status
typedef int Status;
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef None
#undef None
#endif
#ifdef FocusIn
#undef FocusIn
#endif
#ifdef FocusOut
#undef FocusOut
#endif
#endif

#include "display_properties.h"
#include "display_state.h"
#include "display_list_dialog.h"
#include "display_window.h"
#include "legacy_fonts.h"
#include "main_window_controller.h"
#include "object_palette_dialog.h"
#include "statistics_window.h"
#include "window_utils.h"
#include "cursor_utils.h"

namespace {

constexpr char kVersionString[] =
    "QtEDM Version 1.0.0  (EPICS 7.0.9.1-DEV)";

struct GeometrySpec {
  bool hasWidth = false;
  bool hasHeight = false;
  bool hasX = false;
  bool hasY = false;
  int width = 0;
  int height = 0;
  int x = 0;
  int y = 0;
  bool xFromRight = false;
  bool yFromBottom = false;
};

enum class RemoteMode {
  kLocal,
  kAttach,
  kCleanup,
};

struct CommandLineOptions {
  bool startInExecuteMode = false;
  bool showHelp = false;
  bool showVersion = false;
  bool raiseMessageWindow = true;
  bool usePrivateColormap = false;
  bool useBigMousePointer = false;
  bool testSave = false;
  QString invalidOption;
  QStringList displayFiles;
  QString displayGeometry;
  QString macroString;
  RemoteMode remoteMode = RemoteMode::kLocal;
  QStringList resolvedDisplayFiles;
  QString displayFont = QStringLiteral("alias");
};

QString programName(const QStringList &args)
{
  if (args.isEmpty()) {
    return QStringLiteral("qtedm");
  }
  return QFileInfo(args.first()).fileName();
}

void printUsage(const QString &program)
{
  fprintf(stdout, "\n%s\n", kVersionString);
  fprintf(stdout,
      "Usage:\n"
      "  %s [X options]\n"
      "  [-help | -h | -?]\n"
      "  [-version]\n"
      "  [-x]\n"
      "  [-local | -attach | -cleanup]\n"
      "  [-macro \"xxx=aaa,yyy=bbb, ...\"]\n"
      "  [-dg geometry]\n"
      "  [-displayFont alias|scalable]\n"
      "  [-noMsg]\n"
      "  [display-files]\n"
      "  [&]\n"
      "\n",
      program.toLocal8Bit().constData());
  fflush(stdout);
}

CommandLineOptions parseCommandLine(const QStringList &args)
{
  CommandLineOptions options;
  for (int i = 1; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    if (arg == QLatin1String("-x")) {
      options.startInExecuteMode = true;
    } else if (arg == QLatin1String("-local")) {
      options.remoteMode = RemoteMode::kLocal;
    } else if (arg == QLatin1String("-attach")) {
      options.remoteMode = RemoteMode::kAttach;
    } else if (arg == QLatin1String("-cleanup")) {
      options.remoteMode = RemoteMode::kCleanup;
    } else if (arg == QLatin1String("-help") ||
               arg == QLatin1String("-h") ||
               arg == QLatin1String("-?")) {
      options.showHelp = true;
    } else if (arg == QLatin1String("-version")) {
      options.showVersion = true;
    } else if (arg == QLatin1String("-noMsg")) {
      options.raiseMessageWindow = false;
    } else if (arg == QLatin1String("-bigMousePointer")) {
      options.useBigMousePointer = true;
    } else if (arg == QLatin1String("-cmap")) {
      options.usePrivateColormap = true;
    } else if (arg == QLatin1String("-testSave")) {
      options.testSave = true;
    } else if (arg == QLatin1String("-macro")) {
      if ((i + 1) < args.size()) {
        QString tmp = args.at(++i);
        if (!tmp.isEmpty() && tmp.front() == QLatin1Char('"')) {
          tmp.remove(0, 1);
        }
        if (!tmp.isEmpty() && tmp.back() == QLatin1Char('"')) {
          tmp.chop(1);
        }
        options.macroString = tmp.trimmed();
      }
    } else if (arg == QLatin1String("-displayGeometry") ||
               arg == QLatin1String("-dg")) {
      if ((i + 1) < args.size()) {
        options.displayGeometry = args.at(++i);
      }
    } else if (arg == QLatin1String("-displayFont")) {
      if ((i + 1) < args.size()) {
        options.displayFont = args.at(++i).trimmed();
      } else {
        options.invalidOption = arg;
      }
    } else if (arg.startsWith(QLatin1Char('-'))) {
      options.invalidOption = arg;
    } else {
      options.displayFiles.push_back(arg);
    }
  }
  if (!options.invalidOption.isEmpty()) {
    options.showHelp = true;
  }
  if (options.showHelp) {
    options.showVersion = true;
  }
  return options;
}

QStringList displaySearchPaths()
{
  QStringList searchPaths;
  const QByteArray env = qgetenv("EPICS_DISPLAY_PATH");
  if (!env.isEmpty()) {
    const QStringList parts = QString::fromLocal8Bit(env).split(
        QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
      const QString trimmed = part.trimmed();
      if (!trimmed.isEmpty()) {
        searchPaths.push_back(trimmed);
      }
    }
  }
  return searchPaths;
}

QString resolveDisplayFile(const QString &fileArgument)
{
  QFileInfo directInfo(fileArgument);
  if (directInfo.exists() && directInfo.isFile()) {
    return directInfo.absoluteFilePath();
  }
  const QStringList searchPaths = displaySearchPaths();
  for (const QString &basePath : searchPaths) {
    QFileInfo candidate(QDir(basePath), fileArgument);
    if (candidate.exists() && candidate.isFile()) {
      return candidate.absoluteFilePath();
    }
  }
  return QString();
}

QStringList resolveDisplayArguments(const QStringList &files)
{
  QStringList resolved;
  for (const QString &file : files) {
    if (!file.endsWith(QStringLiteral(".adl"), Qt::CaseInsensitive)) {
      fprintf(stderr, "\nFile has wrong suffix: %s\n",
          file.toLocal8Bit().constData());
      fflush(stderr);
      continue;
    }
    const QString resolvedPath = resolveDisplayFile(file);
    if (resolvedPath.isEmpty()) {
      fprintf(stderr, "\nCannot access file: %s\n",
          file.toLocal8Bit().constData());
      fflush(stderr);
      continue;
    }
    resolved.push_back(resolvedPath);
  }
  return resolved;
}

using MacroMap = QHash<QString, QString>;

MacroMap parseMacroDefinitionString(const QString &macroString)
{
  MacroMap macros;
  if (macroString.isEmpty()) {
    return macros;
  }
  const QStringList entries = macroString.split(QLatin1Char(','), Qt::KeepEmptyParts);
  for (const QString &entry : entries) {
    const QString trimmedEntry = entry.trimmed();
    if (trimmedEntry.isEmpty()) {
      continue;
    }
    const int equalsIndex = trimmedEntry.indexOf(QLatin1Char('='));
    if (equalsIndex <= 0) {
      fprintf(stderr, "\nInvalid macro definition: %s\n",
          trimmedEntry.toLocal8Bit().constData());
      continue;
    }
    const QString name = trimmedEntry.left(equalsIndex).trimmed();
    const QString value = trimmedEntry.mid(equalsIndex + 1).trimmed();
    if (name.isEmpty()) {
      fprintf(stderr, "\nInvalid macro definition: %s\n",
          trimmedEntry.toLocal8Bit().constData());
      continue;
    }
    macros.insert(name, value);
  }
  return macros;
}

std::optional<GeometrySpec> geometrySpecFromString(const QString &geometry)
{
  const QString trimmed = geometry.trimmed();
  if (trimmed.isEmpty()) {
    return std::nullopt;
  }
  QString effective = trimmed;
  if (effective.startsWith(QLatin1Char('='))) {
    effective.remove(0, 1);
  }
  static const QRegularExpression kGeometryPattern(
      QStringLiteral(R"(^\s*(?:(\d+))?(?:x(\d+))?([+-]\d+)?([+-]\d+)?\s*$)"));
  const QRegularExpressionMatch match = kGeometryPattern.match(effective);
  if (!match.hasMatch()) {
    return std::nullopt;
  }

  GeometrySpec spec;
  if (!match.captured(1).isEmpty()) {
    spec.hasWidth = true;
    spec.width = match.captured(1).toInt();
  }
  if (!match.captured(2).isEmpty()) {
    spec.hasHeight = true;
    spec.height = match.captured(2).toInt();
  }
  if (!match.captured(3).isEmpty()) {
    spec.hasX = true;
    const QString xStr = match.captured(3);
    spec.xFromRight = xStr.startsWith(QLatin1Char('-'));
    spec.x = xStr.mid(1).toInt();
  }
  if (!match.captured(4).isEmpty()) {
    spec.hasY = true;
    const QString yStr = match.captured(4);
    spec.yFromBottom = yStr.startsWith(QLatin1Char('-'));
    spec.y = yStr.mid(1).toInt();
  }
  return spec;
}

void applyCommandLineGeometry(DisplayWindow *window, const GeometrySpec &spec)
{
  if (!window) {
    return;
  }

  auto resolveScreen = [window]() -> QScreen * {
    QScreen *screen = window->screen();
    if (!screen) {
      screen = QGuiApplication::primaryScreen();
    }
    return screen;
  };

  if (spec.hasWidth || spec.hasHeight) {
    if (auto *displayArea =
            window->findChild<QWidget *>(QStringLiteral("displayArea"))) {
      const QSize previousWindowSize = window->size();
      const QSize previousAreaSize = displayArea->size();
      const int extraWidth =
          previousWindowSize.width() - previousAreaSize.width();
      const int extraHeight =
          previousWindowSize.height() - previousAreaSize.height();
      int targetWidth = previousAreaSize.width();
      int targetHeight = previousAreaSize.height();
      if (spec.hasWidth && spec.width > 0) {
        targetWidth = spec.width;
      }
      if (spec.hasHeight && spec.height > 0) {
        targetHeight = spec.height;
      }
      displayArea->setMinimumSize(targetWidth, targetHeight);
      displayArea->resize(targetWidth, targetHeight);
      window->resize(targetWidth + extraWidth, targetHeight + extraHeight);
    } else {
      QSize target = window->size();
      if (spec.hasWidth && spec.width > 0) {
        target.setWidth(spec.width);
      }
      if (spec.hasHeight && spec.height > 0) {
        target.setHeight(spec.height);
      }
      window->resize(target);
    }
  }

  if (spec.hasX || spec.hasY) {
    auto computeTarget = [&](const QSize &frameSize, const QRect &screenGeometry) {
      QPoint target = window->pos();
      if (spec.hasX) {
        if (spec.xFromRight) {
          target.setX(screenGeometry.x() + screenGeometry.width() -
                      frameSize.width() - spec.x);
        } else {
          target.setX(screenGeometry.x() + spec.x);
        }
      }
      if (spec.hasY) {
        if (spec.yFromBottom) {
          target.setY(screenGeometry.y() + screenGeometry.height() -
                      frameSize.height() - spec.y);
        } else {
          target.setY(screenGeometry.y() + spec.y);
        }
      }
      return target;
    };

    if (QScreen *screen = resolveScreen(); screen && !spec.xFromRight &&
        !spec.yFromBottom) {
      const QRect screenGeometry = screen->geometry();
      const QSize frameSize = window->frameGeometry().size();
      window->move(computeTarget(frameSize, screenGeometry));
    }

    auto moveWindow = [window, spec, resolveScreen, computeTarget]() {
      QScreen *screen = resolveScreen();
      if (!screen) {
        return;
      }
      const QRect screenGeometry = screen->geometry();
      const QSize frameSize = window->frameGeometry().size();
      window->move(computeTarget(frameSize, screenGeometry));
    };
    if (window->isVisible()) {
      moveWindow();
    } else {
      QTimer::singleShot(0, window, moveWindow);
    }
  }
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
constexpr int kMaxCharsInClientMessage = 20;

int ignoreXErrorHandler(Display *, XErrorEvent *)
{
  return 0;
}

QByteArray remotePropertyName(const CommandLineOptions &options)
{
  const char *suffix =
      options.startInExecuteMode ? "_EXEC_FIXED" : "_EDIT_FIXED";
#ifdef MEDM_VERSION_DIGITS
  QByteArray base(MEDM_VERSION_DIGITS);
#else
  QByteArray base("QTEDM010000");
#endif
  base += suffix;
  return base;
}

struct RemoteContext {
  RemoteMode mode = RemoteMode::kLocal;
  Display *display = nullptr;
  Window rootWindow = 0;
  Atom propertyAtom = 0;
  Window existingWindow = 0;
  Window hostWindow = 0;
  bool active = false;
  bool propertyRegistered = false;
};

void sendRemoteRequestMessages(Display *display, Window targetWindow, Atom atom,
    const QString &fullPathName, const QString &macroString,
    const QString &geometryString)
{
  if (!display || targetWindow == 0 || atom == 0) {
    return;
  }

  XClientMessageEvent clientMessageEvent;
  std::memset(&clientMessageEvent, 0, sizeof(clientMessageEvent));
  clientMessageEvent.type = ClientMessage;
  clientMessageEvent.serial = 0;
  clientMessageEvent.send_event = True;
  clientMessageEvent.display = display;
  clientMessageEvent.window = targetWindow;
  clientMessageEvent.message_type = atom;
  clientMessageEvent.format = 8;

  const QByteArray pathBytes = QFile::encodeName(fullPathName);
  const QByteArray macroBytes =
      macroString.isEmpty() ? QByteArray() : QByteArray(macroString.toLocal8Bit());
  const QByteArray geometryBytes = geometryString.isEmpty()
      ? QByteArray()
      : QByteArray(geometryString.toLocal8Bit());

  int index = 0;
  auto flushEvent = [&]() {
    XSendEvent(display, targetWindow, True, NoEventMask,
        reinterpret_cast<XEvent *>(&clientMessageEvent));
  };
  auto appendChar = [&](char ch) {
    if (index == kMaxCharsInClientMessage) {
      flushEvent();
      index = 0;
    }
    clientMessageEvent.data.b[index++] = ch;
  };
  auto appendString = [&](const QByteArray &bytes) {
    for (char ch : bytes) {
      appendChar(ch);
    }
  };

  appendChar('(');
  appendString(pathBytes);
  appendChar(';');
  appendString(macroBytes);
  appendChar(';');
  appendString(geometryBytes);
  appendChar(')');
  for (int i = index; i < kMaxCharsInClientMessage; ++i) {
    clientMessageEvent.data.b[i] = ' ';
  }
  flushEvent();
  XFlush(display);
}

class RemoteRequestFilter : public QAbstractNativeEventFilter {
 public:
  using RequestHandler =
      std::function<void(const QString &, const QString &, const QString &)>;

  RemoteRequestFilter(Atom propertyAtom, Window hostWindow,
      RequestHandler handler)
    : propertyAtom_(propertyAtom)
    , hostWindow_(hostWindow)
    , handler_(std::move(handler))
  {}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  bool nativeEventFilter(const QByteArray &eventType, void *message,
      qintptr *result) override
#else
  bool nativeEventFilter(const QByteArray &eventType, void *message,
      long *result) override
#endif
  {
    Q_UNUSED(result);
    if (eventType != QByteArrayLiteral("xcb_generic_event_t")) {
      return false;
    }
    auto *genericEvent =
        static_cast<xcb_generic_event_t *>(message);
    const uint8_t responseType = genericEvent->response_type & ~0x80;
    if (responseType != XCB_CLIENT_MESSAGE) {
      return false;
    }
    auto *clientMessage =
        reinterpret_cast<xcb_client_message_event_t *>(genericEvent);
    if (clientMessage->type != propertyAtom_) {
      return false;
    }
    if (clientMessage->window != hostWindow_) {
      return false;
    }
    const char *data =
        reinterpret_cast<const char *>(clientMessage->data.data8);
    for (int i = 0; i < kMaxCharsInClientMessage; ++i) {
      const char ch = data[i];
      if (ch == '(') {
        collecting_ = true;
        messageClass_ = MessageClass::kFilename;
        filenameBuffer_.clear();
        macroBuffer_.clear();
        geometryBuffer_.clear();
        continue;
      }
      if (!collecting_) {
        continue;
      }
      if (ch == ';') {
        if (messageClass_ == MessageClass::kFilename) {
          messageClass_ = MessageClass::kMacro;
        } else {
          messageClass_ = MessageClass::kGeometry;
        }
        continue;
      }
      if (ch == ')') {
        collecting_ = false;
        messageClass_ = MessageClass::kNone;
        if (handler_) {
          const QString filename =
              QString::fromLocal8Bit(filenameBuffer_.constData(),
                  filenameBuffer_.size());
          const QString macro =
              QString::fromLocal8Bit(macroBuffer_.constData(),
                  macroBuffer_.size());
          const QString geometry =
              QString::fromLocal8Bit(geometryBuffer_.constData(),
                  geometryBuffer_.size());
          handler_(filename, macro, geometry);
        }
        continue;
      }
      if (ch == '\0') {
        continue;
      }
      switch (messageClass_) {
      case MessageClass::kFilename:
        filenameBuffer_.append(ch);
        break;
      case MessageClass::kMacro:
        macroBuffer_.append(ch);
        break;
      case MessageClass::kGeometry:
        geometryBuffer_.append(ch);
        break;
      case MessageClass::kNone:
        break;
      }
    }
    return false;
  }

 private:
  enum class MessageClass {
    kNone,
    kFilename,
    kMacro,
    kGeometry,
  };

  Atom propertyAtom_ = 0;
  Window hostWindow_ = 0;
  RequestHandler handler_;
  bool collecting_ = false;
  MessageClass messageClass_ = MessageClass::kNone;
  QByteArray filenameBuffer_;
  QByteArray macroBuffer_;
  QByteArray geometryBuffer_;
};
#endif  // defined(Q_OS_UNIX) && !defined(Q_OS_MAC)

}  // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  // High-DPI is on by default in Qt6
#else
  // Opt-in for sensible DPI scaling on Qt5
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

  QApplication app(argc, argv);
  Q_INIT_RESOURCE(icons);

  const QStringList args = QCoreApplication::arguments();
  CommandLineOptions options = parseCommandLine(args);
  options.resolvedDisplayFiles = resolveDisplayArguments(options.displayFiles);
  const std::optional<GeometrySpec> geometrySpec =
      geometrySpecFromString(options.displayGeometry);
  if (options.testSave) {
    options.startInExecuteMode = false;
    options.remoteMode = RemoteMode::kLocal;
  }
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
  RemoteContext remoteContext;
  remoteContext.mode = options.remoteMode;
  std::unique_ptr<RemoteRequestFilter> remoteFilter;
#else
  if (options.remoteMode != RemoteMode::kLocal) {
    fprintf(stdout,
        "\nRemote control options are only supported on X11 platforms."
        " Proceeding in local mode.\n");
    fflush(stdout);
    options.remoteMode = RemoteMode::kLocal;
  }
#endif

  if (!options.invalidOption.isEmpty()) {
    fprintf(stderr, "\nInvalid option: %s\n",
        options.invalidOption.toLocal8Bit().constData());
    fflush(stderr);
  }

  if (options.showHelp) {
    printUsage(programName(args));
    return 0;
  }

  if (!options.macroString.isEmpty() && !options.startInExecuteMode) {
    fprintf(stdout, "\nIgnored -macro command line option\n"
                    "  (Only valid for Execute (-x) mode operation)\n");
    fflush(stdout);
    options.macroString.clear();
  }
  CursorUtils::setUseBigCursor(options.useBigMousePointer);

  if (!options.displayGeometry.isEmpty() && !options.displayFiles.isEmpty() &&
      !geometrySpec) {
    fprintf(stderr, "\nInvalid geometry: %s\n",
        options.displayGeometry.toLocal8Bit().constData());
    fflush(stderr);
    printUsage(programName(args));
    return 1;
  }

  if (options.testSave && options.resolvedDisplayFiles.isEmpty()) {
    fprintf(stderr, "\n-testSave requires at least one ADL file argument\n");
    fflush(stderr);
    return 1;
  }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
  if (!options.showVersion && remoteContext.mode != RemoteMode::kLocal) {
    if (QGuiApplication::platformName() != QLatin1String("xcb")) {
      fprintf(stdout,
          "\nRemote control options require an X11 platform. Proceeding in local mode.\n");
      fflush(stdout);
      options.remoteMode = RemoteMode::kLocal;
      remoteContext.mode = RemoteMode::kLocal;
    } else {
      remoteContext.display = XOpenDisplay(nullptr);
      if (!remoteContext.display) {
        fprintf(stdout,
            "\nCannot access X11 display connection. Proceeding in local mode.\n");
        fflush(stdout);
        options.remoteMode = RemoteMode::kLocal;
        remoteContext.mode = RemoteMode::kLocal;
      } else {
        remoteContext.active = true;
        remoteContext.rootWindow = DefaultRootWindow(remoteContext.display);
        const QByteArray propertyName = remotePropertyName(options);
        remoteContext.propertyAtom = XInternAtom(remoteContext.display,
            propertyName.constData(), False);
        Atom type = 0;
        int format = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *propertyData = nullptr;
        Status status = XGetWindowProperty(remoteContext.display,
            remoteContext.rootWindow, remoteContext.propertyAtom, 0, PATH_MAX,
            False, AnyPropertyType, &type, &format, &nitems, &bytesAfter,
            &propertyData);
        if (status == Success && type != 0 && propertyData &&
            format == 32 && nitems > 0) {
          remoteContext.existingWindow =
              reinterpret_cast<Window *>(propertyData)[0];
        }
        if (propertyData) {
          XFree(propertyData);
        }

        bool attachToExisting =
            remoteContext.mode == RemoteMode::kAttach &&
            remoteContext.existingWindow != 0;
        if (attachToExisting) {
          XWindowAttributes attributes;
          auto previousHandler =
              XSetErrorHandler(ignoreXErrorHandler);
          Status attributeStatus = XGetWindowAttributes(remoteContext.display,
              remoteContext.existingWindow, &attributes);
          XSetErrorHandler(previousHandler);
          if (!attributeStatus) {
            fprintf(stdout,
                "\nCannot connect to existing QtEDM because it is invalid\n"
                "  (An accompanying Bad Window error can be ignored)\n"
                "  Continuing with this one as if -cleanup were specified\n");
            fprintf(stdout,
                "(Use -local to not use existing QtEDM or be available as an existing QtEDM\n"
                "  or -cleanup to set this QtEDM as the existing one)\n");
            fflush(stdout);
          } else {
            if (options.resolvedDisplayFiles.isEmpty()) {
              fprintf(stdout,
                  "\nAborting: No valid display specified and already "
                  "a remote QtEDM running.\n");
              fprintf(stdout,
                  "(Use -local to not use existing QtEDM or be available as an existing QtEDM\n"
                  "  or -cleanup to set this QtEDM as the existing one)\n");
              fflush(stdout);
              return 0;
            }
            fprintf(stdout, "\nAttaching to existing QtEDM\n");
            for (const QString &resolved : options.resolvedDisplayFiles) {
              sendRemoteRequestMessages(remoteContext.display,
                  remoteContext.existingWindow, remoteContext.propertyAtom,
                  resolved, options.macroString, options.displayGeometry);
              fprintf(stdout, "  Dispatched: %s\n",
                  resolved.toLocal8Bit().constData());
            }
            fprintf(stdout,
                "(Use -local to not use existing QtEDM or be available as an existing QtEDM\n"
                "  or -cleanup to set this QtEDM as the existing one)\n");
            fflush(stdout);
            if (remoteContext.display) {
              XCloseDisplay(remoteContext.display);
              remoteContext.display = nullptr;
            }
            return 0;
          }
        }
      }
    }
  }
#endif

  if (options.showVersion) {
    fprintf(stdout, "\n%s\n\n", kVersionString);
    fflush(stdout);
    return 0;
  }

  if (options.displayFont.compare(QStringLiteral("scalable"), Qt::CaseInsensitive) == 0) {
    LegacyFonts::setWidgetDMAliasMode(LegacyFonts::WidgetDMAliasMode::kScalable);
  } else if (options.displayFont.isEmpty() ||
             options.displayFont.compare(QStringLiteral("alias"), Qt::CaseInsensitive) == 0) {
    LegacyFonts::setWidgetDMAliasMode(LegacyFonts::WidgetDMAliasMode::kFixed);
  } else {
    fprintf(stdout, "\nUnsupported display font specification: %s\n"
                    "  Falling back to alias fonts.\n",
        options.displayFont.toLocal8Bit().constData());
    fflush(stdout);
    LegacyFonts::setWidgetDMAliasMode(LegacyFonts::WidgetDMAliasMode::kFixed);
  }

  if (auto *fusionStyle =
          QStyleFactory::create(QStringLiteral("Fusion"))) {
    app.setStyle(fusionStyle);
  }

  const QFont fixed10Font = LegacyFonts::fontOrDefault(
      QStringLiteral("widgetDM_10"),
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
  app.setFont(fixed10Font);

  const QFont fixed13Font = LegacyFonts::fontOrDefault(
      QStringLiteral("miscFixed13"), fixed10Font);

  QMainWindow win;
  win.setObjectName("QtedmMainWindow");
  win.setWindowTitle("QtEDM");

  const QColor backgroundColor(0xb0, 0xc3, 0xca);
  const QColor highlightColor = backgroundColor.lighter(120);
  const QColor midHighlightColor = backgroundColor.lighter(108);
  const QColor shadowColor = backgroundColor.darker(120);
  const QColor midShadowColor = backgroundColor.darker(140);
  const QColor disabledTextColor(0x64, 0x64, 0x64);
  QPalette palette = win.palette();
  palette.setColor(QPalette::Window, backgroundColor);
  palette.setColor(QPalette::Base, backgroundColor);
  palette.setColor(QPalette::AlternateBase, backgroundColor);
  palette.setColor(QPalette::Button, backgroundColor);
  palette.setColor(QPalette::WindowText, Qt::black);
  palette.setColor(QPalette::ButtonText, Qt::black);
  palette.setColor(QPalette::Light, highlightColor);
  palette.setColor(QPalette::Midlight, midHighlightColor);
  palette.setColor(QPalette::Dark, shadowColor);
  palette.setColor(QPalette::Mid, midShadowColor);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledTextColor);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledTextColor);
  palette.setColor(QPalette::Disabled, QPalette::Text, disabledTextColor);
  palette.setColor(QPalette::Disabled, QPalette::Button, backgroundColor);
  win.setPalette(palette);

  auto *menuBar = win.menuBar();
  menuBar->setAutoFillBackground(true);
  menuBar->setPalette(palette);
  menuBar->setFont(fixed13Font);

  auto *fileMenu = menuBar->addMenu("&File");
  fileMenu->setFont(fixed13Font);
  auto *newAct = fileMenu->addAction("&New");
  newAct->setShortcut(QKeySequence::New);
  auto *openAct = fileMenu->addAction("&Open...");
  openAct->setShortcut(QKeySequence::Open);
  auto *saveAct = fileMenu->addAction("&Save");
  saveAct->setShortcut(QKeySequence::Save);
  auto *saveAsAct = fileMenu->addAction("Save &As...");
  auto *closeAct = fileMenu->addAction("&Close");
  fileMenu->addSeparator();
  auto *printSetupAct = fileMenu->addAction("Print Set&up...");
  auto *printAct = fileMenu->addAction("&Print");
  printAct->setShortcut(QKeySequence::Print);
  fileMenu->addSeparator();
  auto *exitAct = fileMenu->addAction("E&xit");
  exitAct->setShortcut(QKeySequence::Quit);
  QObject::connect(exitAct, &QAction::triggered, &app, &QApplication::quit);
  saveAct->setEnabled(false);
  saveAsAct->setEnabled(false);
  closeAct->setEnabled(false);

  auto *editMenu = menuBar->addMenu("&Edit");
  editMenu->setFont(fixed13Font);
  auto *undoAct = editMenu->addAction("&Undo");
  undoAct->setShortcut(QKeySequence::Undo);
  undoAct->setEnabled(false);
  auto *redoAct = editMenu->addAction("&Redo");
  redoAct->setShortcut(QKeySequence::Redo);
  redoAct->setEnabled(false);
  editMenu->addSeparator();
  auto *cutAct = editMenu->addAction("Cu&t");
  cutAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
  auto *copyAct = editMenu->addAction("&Copy");
  copyAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
  auto *pasteAct = editMenu->addAction("&Paste");
  pasteAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
  editMenu->addSeparator();
  auto *raiseAct = editMenu->addAction("&Raise");
  auto *lowerAct = editMenu->addAction("&Lower");
  editMenu->addSeparator();
  auto *groupAct = editMenu->addAction("&Group");
  auto *ungroupAct = editMenu->addAction("&Ungroup");
  editMenu->addSeparator();
  auto *alignMenu = editMenu->addMenu("&Align");
  alignMenu->setFont(fixed13Font);
  auto *alignLeftAct = alignMenu->addAction("&Left");
  auto *alignHorizontalCenterAct = alignMenu->addAction("&Horizontal Center");
  auto *alignRightAct = alignMenu->addAction("&Right");
  auto *alignTopAct = alignMenu->addAction("&Top");
  auto *alignVerticalCenterAct = alignMenu->addAction("&Vertical Center");
  auto *alignBottomAct = alignMenu->addAction("&Bottom");
  auto *positionToGridAct = alignMenu->addAction("Position to &Grid");
  auto *edgesToGridAct = alignMenu->addAction("Ed&ges to Grid");

  auto *spaceMenu = editMenu->addMenu("Space &Evenly");
  spaceMenu->setFont(fixed13Font);
  auto *spaceHorizontalAct = spaceMenu->addAction("&Horizontal");
  auto *spaceVerticalAct = spaceMenu->addAction("&Vertical");
  auto *space2DAct = spaceMenu->addAction("&2-D");

  auto *centerMenu = editMenu->addMenu("&Center");
  centerMenu->setFont(fixed13Font);
  auto *centerHorizontalAct =
      centerMenu->addAction("&Horizontally in Display");
  auto *centerVerticalAct =
      centerMenu->addAction("&Vertically in Display");
  auto *centerBothAct = centerMenu->addAction("&Both");

  auto *orientMenu = editMenu->addMenu("&Orient");
  orientMenu->setFont(fixed13Font);
  auto *flipHorizontalAct = orientMenu->addAction("Flip &Horizontally");
  auto *flipVerticalAct = orientMenu->addAction("Flip &Vertically");
  auto *rotateClockwiseAct = orientMenu->addAction("Rotate &Clockwise");
  auto *rotateCounterclockwiseAct =
      orientMenu->addAction("Rotate &Counterclockwise");

  auto *sizeMenu = editMenu->addMenu("&Size");
  sizeMenu->setFont(fixed13Font);
  auto *sameSizeAct = sizeMenu->addAction("&Same Size");
  auto *textToContentsAct = sizeMenu->addAction("Text to &Contents");

  auto *gridMenu = editMenu->addMenu("&Grid");
  gridMenu->setFont(fixed13Font);
  auto *toggleGridAct = gridMenu->addAction("Toggle Show &Grid");
  auto *toggleSnapAct = gridMenu->addAction("Toggle &Snap To Grid");
  auto *gridSpacingAct = gridMenu->addAction("Grid &Spacing...");
  toggleGridAct->setCheckable(true);
  toggleSnapAct->setCheckable(true);

  editMenu->addSeparator();
  auto *unselectAct = editMenu->addAction("U&nselect");
  auto *selectAllAct = editMenu->addAction("Select &All");
  auto *selectDisplayAct = editMenu->addAction("Select &Display");
  editMenu->addSeparator();
  auto *findOutliersAct = editMenu->addAction("Find &Outliers");
  auto *refreshAct = editMenu->addAction("&Refresh");
  auto *editSummaryAct = editMenu->addAction("Edit &Summary...");

  editMenu->setEnabled(false);
  editMenu->menuAction()->setEnabled(false);

  auto *viewMenu = menuBar->addMenu("&View");
  viewMenu->setFont(fixed13Font);
  auto *messageWindowAct = viewMenu->addAction("&Message Window");
  messageWindowAct->setEnabled(false);
  auto *statisticsWindowAct = viewMenu->addAction("&Statistics Window");
  auto *viewDisplayListAct = viewMenu->addAction("&Display List");

  auto *palettesMenu = menuBar->addMenu("&Palettes");
  palettesMenu->setFont(fixed13Font);
  auto *objectPaletteAct = palettesMenu->addAction("&Object");
  palettesMenu->addAction("&Resource");
  palettesMenu->addAction("&Color");
  palettesMenu->setEnabled(false);
  palettesMenu->menuAction()->setEnabled(false);

  auto *helpMenu = menuBar->addMenu("&Help");
  helpMenu->setFont(fixed13Font);
  auto *overviewAct = helpMenu->addAction("&Overview");
  QObject::connect(overviewAct, &QAction::triggered, &win,
      [&win, &fixed13Font, &palette]() {
        /* Use embedded resource for help file */
        showHelpBrowser(&win, QStringLiteral("QtEDM Help - Overview"),
            QStringLiteral(":/help/QtEDM.html"), fixed13Font, palette);
      });
  auto *onVersionAct = helpMenu->addAction("&Version");
  QObject::connect(onVersionAct, &QAction::triggered, &win,
      [&win, &fixed13Font, &fixed10Font, &palette]() {
        showVersionDialog(&win, fixed13Font, fixed10Font, palette, false);
      });

  auto *central = new QWidget;
  central->setObjectName("mainBB");
  central->setAutoFillBackground(true);
  central->setPalette(palette);
  central->setBackgroundRole(QPalette::Window);

  auto *layout = new QVBoxLayout;
  layout->setContentsMargins(10, 8, 10, 10);
  layout->setSpacing(10);

  auto *modePanel = new QFrame;
  modePanel->setFrameShape(QFrame::Panel);
  modePanel->setFrameShadow(QFrame::Sunken);
  modePanel->setLineWidth(2);
  modePanel->setMidLineWidth(1);
  modePanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  modePanel->setAutoFillBackground(true);
  modePanel->setPalette(palette);
  modePanel->setBackgroundRole(QPalette::Button);

  auto *panelLayout = new QVBoxLayout(modePanel);
  panelLayout->setContentsMargins(12, 8, 12, 12);
  panelLayout->setSpacing(6);

  auto *modeBox = new QGroupBox("Mode");
  modeBox->setFont(fixed13Font);
  modeBox->setAutoFillBackground(true);
  modeBox->setPalette(palette);
  modeBox->setBackgroundRole(QPalette::Window);
  modeBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  modeBox->setStyleSheet(
      "QGroupBox { border: 2px groove palette(mid); margin-top: 0.8em;"
      " padding: 6px 12px 8px 12px; }"
      " QGroupBox::title { subcontrol-origin: margin; left: 10px;"
      " padding: 0 4px; }");

  auto *modeLayout = new QHBoxLayout;
  modeLayout->setContentsMargins(12, 8, 12, 8);
  modeLayout->setSpacing(14);
  auto *editModeButton = new QRadioButton("Edit");
  auto *executeModeButton = new QRadioButton("Execute");
  editModeButton->setFont(fixed13Font);
  executeModeButton->setFont(fixed13Font);
  modeLayout->addWidget(editModeButton);
  modeLayout->addWidget(executeModeButton);
  auto *modeButtonsWidget = new QWidget;
  modeButtonsWidget->setLayout(modeLayout);
  auto *executeOnlyLabel = new QLabel(QStringLiteral("Execute-Only"));
  executeOnlyLabel->setFont(fixed13Font);
  executeOnlyLabel->setAlignment(Qt::AlignCenter);
  auto *executeOnlyWidget = new QWidget;
  auto *executeOnlyLayout = new QHBoxLayout(executeOnlyWidget);
  executeOnlyLayout->setContentsMargins(12, 8, 12, 8);
  executeOnlyLayout->addStretch();
  executeOnlyLayout->addWidget(executeOnlyLabel, 0, Qt::AlignCenter);
  executeOnlyLayout->addStretch();
  auto *modeStack = new QStackedLayout;
  modeStack->setContentsMargins(0, 0, 0, 0);
  modeStack->addWidget(modeButtonsWidget);
  modeStack->addWidget(executeOnlyWidget);
  modeBox->setLayout(modeStack);

  auto state = std::make_shared<DisplayState>();
  state->mainWindow = &win;
  auto *mainWindowController = new MainWindowController(&win,
      std::weak_ptr<DisplayState>(state));
  win.installEventFilter(mainWindowController);
  auto updateMenus = std::make_shared<std::function<void()>>();
  state->updateMenus = updateMenus;
  auto *displayListDialog = new DisplayListDialog(palette, fixed13Font,
      std::weak_ptr<DisplayState>(state), &win);
  state->displayListDialog = displayListDialog;

//TODO: Add tabbed container / stacked widget support for multi-view displays.

//TODO: Implement 2D image viewer for EPICS areaDetector NDArray PVs.
//TODO: Add 2D contour / heatmap widget (like sddscontour) for SDDS or array PVs.
//TODO: Implement vector/arrow field widget for displaying field maps or quiver data.
//TODO: Add spectrogram / FFT display widget for frequency-domain visualization.
//TODO: Create scrolling log or mini-alarm panel widget with timestamps.

//TODO: Implement modern toggle switch and pushbutton styles (with LED indicators).
//TODO: Add numeric spinbox with PV unit display (from EPICS metadata).
//TODO: Implement logarithmic slider / knob for wide dynamic-range parameters.
//TODO: Create rotary knob / dial widget for continuous analog setpoints.
//TODO: Implement PV table / matrix editor widget for grouped parameter control.
//TODO: Add macro-enabled button widget to execute macros or scripts on click.

//TODO: Implement alarm summary / banner widget showing active alarms inline.
//TODO: Add PV tree / hierarchical browser widget for structured PV navigation.
//TODO: Create embedded WebView widget for docs, Grafana panels, or logs.
//TODO: Design scriptable widget framework (Python or JavaScript per widget).

//TODO: Implement SDDS table viewer widget for displaying tabular SDDS datasets.
//TODO: Add SDDS plot widget for static or live plotting of SDDS data columns.
//TODO: Create lattice / beamline schematic widget for visualizing beamline elements.
//TODO: Implement PV waveform scope widget for fast waveform diagnostics.

//TODO: Add theme/palette system (dark, light, facility-specific branding).
//TODO: Implement dockable layouts so operators can rearrange displays.
//TODO: Add searchable PV inspection mode (show PV name and metadata on click).
//TODO: Implement developer overlay for PV connection state and update rate.

//TODO: Add support for importing caQtDM .ui and CSS .opi display files.
//TODO: Design plugin API for custom widgets (C++ or Python registration).
//TODO: Add EPICS PVAccess (PVA) support alongside Channel Access.
//TODO: Implement versioned schema system for forward/backward compatibility.

//TODO: Phase 1 – Complete MEDM compatibility and core widgets.
//TODO: Phase 2 – Add visualization widgets and areaDetector integration.
//TODO: Phase 3 – Introduce scripting and SDDS widget extensions.
//TODO: Phase 4 – Improve operator UX and theming support.
//TODO: Phase 5 – Develop plugin framework and PVA integration.


  auto objectPaletteDialog = QPointer<ObjectPaletteDialog>(
      new ObjectPaletteDialog(palette, fixed13Font, fixed10Font,
          std::weak_ptr<DisplayState>(state), &win));

  auto statisticsWindow = QPointer<StatisticsWindow>(
    new StatisticsWindow(palette, fixed13Font, fixed10Font, &win));
  state->raiseMessageWindow = options.raiseMessageWindow;

  QObject::connect(viewDisplayListAct, &QAction::triggered, displayListDialog,
      [displayListDialog]() {
        displayListDialog->showAndRaise();
      });

  QObject::connect(objectPaletteAct, &QAction::triggered,
      objectPaletteDialog.data(), [objectPaletteDialog]() {
        if (objectPaletteDialog) {
          objectPaletteDialog->showAndRaise();
        }
      });

  QObject::connect(statisticsWindowAct, &QAction::triggered, &win,
      [statisticsWindow]() {
        if (statisticsWindow) {
          statisticsWindow->showAndRaise();
        }
      });

  QObject::connect(saveAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->save();
        }
      });
  QObject::connect(printSetupAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->showPrintSetup();
        } else if (!state->displays.isEmpty()) {
          if (auto firstDisplay = state->displays.first().data()) {
            firstDisplay->showPrintSetup();
          }
        }
      });
  QObject::connect(printAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->printDisplay();
        } else if (!state->displays.isEmpty()) {
          if (auto firstDisplay = state->displays.first().data()) {
            firstDisplay->printDisplay();
          }
        }
      });
  QObject::connect(undoAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          if (auto *stack = active->undoStack()) {
            stack->undo();
          }
        }
      });
  QObject::connect(redoAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          if (auto *stack = active->undoStack()) {
            stack->redo();
          }
        }
      });
  QObject::connect(cutAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerCutFromMenu();
        }
      });
  QObject::connect(copyAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerCopyFromMenu();
        }
      });
  QObject::connect(pasteAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerPasteFromMenu();
        }
      });
  QObject::connect(raiseAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->raiseSelection();
        }
      });
  QObject::connect(lowerAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->lowerSelection();
        }
      });
  QObject::connect(groupAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerGroupFromMenu();
        }
      });
  QObject::connect(ungroupAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerUngroupFromMenu();
        }
      });
  QObject::connect(alignLeftAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionLeft();
        }
      });
  QObject::connect(alignHorizontalCenterAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionHorizontalCenter();
        }
      });
  QObject::connect(alignRightAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionRight();
        }
      });
  QObject::connect(alignTopAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionTop();
        }
      });
  QObject::connect(alignVerticalCenterAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionVerticalCenter();
        }
      });
  QObject::connect(alignBottomAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionBottom();
        }
      });
  QObject::connect(positionToGridAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionPositionToGrid();
        }
      });
  QObject::connect(edgesToGridAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionEdgesToGrid();
        }
      });
  QObject::connect(spaceHorizontalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->spaceSelectionHorizontal();
        }
      });
  QObject::connect(spaceVerticalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->spaceSelectionVertical();
        }
      });
  QObject::connect(space2DAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->spaceSelection2D();
        }
      });
  QObject::connect(centerHorizontalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->centerSelectionHorizontallyInDisplay();
        }
      });
  QObject::connect(centerVerticalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->centerSelectionVerticallyInDisplay();
        }
      });
  QObject::connect(centerBothAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->centerSelectionInDisplayBoth();
        }
      });
  QObject::connect(flipHorizontalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->orientSelectionFlipHorizontal();
        }
      });
  QObject::connect(flipVerticalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->orientSelectionFlipVertical();
        }
      });
  QObject::connect(rotateClockwiseAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->rotateSelectionClockwise();
        }
      });
  QObject::connect(rotateCounterclockwiseAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->rotateSelectionCounterclockwise();
        }
      });
  QObject::connect(sameSizeAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->sizeSelectionSameSize();
        }
      });
  QObject::connect(textToContentsAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->sizeSelectionTextToContents();
        }
      });
  QObject::connect(toggleGridAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->setGridOn(!active->isGridOn());
        }
      });
  QObject::connect(toggleSnapAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->setSnapToGrid(!active->isSnapToGridEnabled());
        }
      });
  QObject::connect(gridSpacingAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->promptForGridSpacing();
        }
      });
  QObject::connect(unselectAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->clearSelection();
        }
      });
  QObject::connect(selectAllAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->selectAllElements();
        }
      });
  QObject::connect(selectDisplayAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->selectDisplayElement();
        }
      });
  QObject::connect(findOutliersAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->findOutliers();
        }
      });
  QObject::connect(refreshAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->refreshDisplayView();
        }
      });
  QObject::connect(editSummaryAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->showEditSummaryDialog();
        }
      });
  QObject::connect(saveAsAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->saveAs();
        }
      });
  QObject::connect(closeAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->close();
        }
      });

  QPalette displayPalette = palette;
  const QColor displayBackgroundColor(0xbb, 0xbb, 0xbb);
  displayPalette.setColor(QPalette::Window, displayBackgroundColor);
  displayPalette.setColor(QPalette::Base, displayBackgroundColor);
  displayPalette.setColor(QPalette::AlternateBase, displayBackgroundColor);
  displayPalette.setColor(QPalette::Button, displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::Window,
      displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::Base,
      displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::AlternateBase,
      displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::Button,
      displayBackgroundColor);

  *updateMenus = [state, editMenu, palettesMenu, newAct, saveAct, saveAsAct,
    closeAct, undoAct, redoAct, cutAct, copyAct, pasteAct, raiseAct, lowerAct, groupAct,
    ungroupAct, alignLeftAct, alignHorizontalCenterAct, alignRightAct,
    alignTopAct, alignVerticalCenterAct, alignBottomAct, positionToGridAct,
    edgesToGridAct, spaceHorizontalAct, spaceVerticalAct, space2DAct,
    centerHorizontalAct, centerVerticalAct, centerBothAct,
    flipHorizontalAct, flipVerticalAct, rotateClockwiseAct,
    rotateCounterclockwiseAct, sameSizeAct, textToContentsAct, toggleGridAct, toggleSnapAct,
    gridSpacingAct, unselectAct, selectAllAct, selectDisplayAct,
    findOutliersAct, refreshAct, editSummaryAct, displayListDialog,
    objectPaletteDialog]() {
    auto &displays = state->displays;
    for (auto it = displays.begin(); it != displays.end();) {
      if (it->isNull()) {
        it = displays.erase(it);
      } else {
        ++it;
      }
    }

    if (!state->activeDisplay.isNull()) {
      bool found = false;
      for (const auto &display : displays) {
        if (display == state->activeDisplay) {
          found = true;
          break;
        }
      }
      if (!found) {
        state->activeDisplay.clear();
      }
    }

    DisplayWindow *active = state->activeDisplay.data();
    if (!active) {
      for (auto it = displays.crbegin(); it != displays.crend(); ++it) {
        if (!it->isNull()) {
          active = it->data();
          state->activeDisplay = active;
          break;
        }
      }
    }

    const bool hasDisplay = !displays.isEmpty();
    const bool enableEditing = hasDisplay && state->editMode;
    const bool canEditActive = enableEditing && active;

    editMenu->setEnabled(enableEditing);
    editMenu->menuAction()->setEnabled(enableEditing);
    palettesMenu->setEnabled(enableEditing);
    palettesMenu->menuAction()->setEnabled(enableEditing);
    newAct->setEnabled(state->editMode);
    saveAct->setEnabled(canEditActive && active->isDirty());
    saveAsAct->setEnabled(canEditActive);
    closeAct->setEnabled(active);
    QString undoTextLabel = QStringLiteral("&Undo");
    bool enableUndo = false;
    if (canEditActive && active) {
      if (auto *stack = active->undoStack()) {
        if (stack->canUndo()) {
          enableUndo = true;
          const QString stackText = stack->undoText();
          if (!stackText.isEmpty()) {
            undoTextLabel = QStringLiteral("&Undo %1").arg(stackText);
          }
        }
      }
    }
    undoAct->setEnabled(enableUndo);
    undoAct->setText(undoTextLabel);
    QString redoTextLabel = QStringLiteral("&Redo");
    bool enableRedo = false;
    if (canEditActive && active) {
      if (auto *stack = active->undoStack()) {
        if (stack->canRedo()) {
          enableRedo = true;
          const QString stackText = stack->redoText();
          if (!stackText.isEmpty()) {
            redoTextLabel = QStringLiteral("&Redo %1").arg(stackText);
          }
        }
      }
    }
    redoAct->setEnabled(enableRedo);
    redoAct->setText(redoTextLabel);
    const bool hasSelection = canEditActive && active
        && active->hasCopyableSelection();
    cutAct->setEnabled(hasSelection);
    copyAct->setEnabled(hasSelection);
    const bool canPaste = canEditActive && active && active->canPaste();
    pasteAct->setEnabled(canPaste);
    const bool canRaise = canEditActive && active && active->canRaiseSelection();
    const bool canLower = canEditActive && active && active->canLowerSelection();
    raiseAct->setEnabled(canRaise);
    lowerAct->setEnabled(canLower);
    const bool canGroup = canEditActive && active && active->canGroupSelection();
    toggleGridAct->setEnabled(canEditActive);
    toggleGridAct->setChecked(canEditActive && active && active->isGridOn());
    toggleSnapAct->setEnabled(canEditActive);
    toggleSnapAct->setChecked(canEditActive && active && active->isSnapToGridEnabled());
    gridSpacingAct->setEnabled(canEditActive);
    const bool canUngroup = canEditActive && active && active->canUngroupSelection();
    groupAct->setEnabled(canGroup);
    ungroupAct->setEnabled(canUngroup);
    const bool canAlign = canEditActive && active && active->canAlignSelection();
    alignLeftAct->setEnabled(canAlign);
    alignHorizontalCenterAct->setEnabled(canAlign);
    alignRightAct->setEnabled(canAlign);
    alignTopAct->setEnabled(canAlign);
    alignVerticalCenterAct->setEnabled(canAlign);
    alignBottomAct->setEnabled(canAlign);
    const bool canAlignToGrid = canEditActive && active
        && active->canAlignSelectionToGrid();
    positionToGridAct->setEnabled(canAlignToGrid);
    edgesToGridAct->setEnabled(canAlignToGrid);
    const bool canSpace = canEditActive && active
        && active->canSpaceSelection();
    const bool canSpace2D = canEditActive && active
        && active->canSpaceSelection2D();
    spaceHorizontalAct->setEnabled(canSpace);
    spaceVerticalAct->setEnabled(canSpace);
    space2DAct->setEnabled(canSpace2D);
    const bool canCenter = canEditActive && active
        && active->canCenterSelection();
    centerHorizontalAct->setEnabled(canCenter);
    centerVerticalAct->setEnabled(canCenter);
    centerBothAct->setEnabled(canCenter);
    const bool canOrient = canEditActive && active
        && active->canOrientSelection();
    flipHorizontalAct->setEnabled(canOrient);
    flipVerticalAct->setEnabled(canOrient);
    rotateClockwiseAct->setEnabled(canOrient);
    rotateCounterclockwiseAct->setEnabled(canOrient);
    const bool canSizeSame = canEditActive && active
        && active->canSizeSelectionSameSize();
    const bool canSizeContents = canEditActive && active
        && active->canSizeSelectionTextToContents();
    sameSizeAct->setEnabled(canSizeSame);
    textToContentsAct->setEnabled(canSizeContents);
    const bool canOperateSelection = canEditActive && active;
    unselectAct->setEnabled(canOperateSelection);
    selectAllAct->setEnabled(canOperateSelection);
    selectDisplayAct->setEnabled(canOperateSelection);
    findOutliersAct->setEnabled(canOperateSelection);
    refreshAct->setEnabled(canOperateSelection);
    editSummaryAct->setEnabled(canOperateSelection);

    if (displayListDialog) {
      displayListDialog->handleStateChanged();
    }
    if (objectPaletteDialog) {
      objectPaletteDialog->refreshSelectionFromState();
    }
  };

  auto registerDisplayWindow = [state, updateMenus, &win](
      DisplayWindow *displayWin) {
    state->displays.append(displayWin);
    displayWin->syncCreateCursor();

    QObject::connect(displayWin, &QObject::destroyed, &win,
        [state, updateMenus,
            displayPtr = QPointer<DisplayWindow>(displayWin)]() {
          if (state) {
            if (state->activeDisplay == displayPtr) {
              state->activeDisplay.clear();
            }
            bool hasLiveDisplay = false;
            for (auto &display : state->displays) {
              if (!display.isNull()) {
                hasLiveDisplay = true;
                break;
              }
            }
            if (!hasLiveDisplay) {
              state->createTool = CreateTool::kNone;
            }
          }
          if (updateMenus && *updateMenus) {
            (*updateMenus)();
          }
        });

    displayWin->show();
    displayWin->raise();
    displayWin->activateWindow();
    displayWin->handleEditModeChanged(state->editMode);

    if (updateMenus && *updateMenus) {
      (*updateMenus)();
    }
  };

  /* Set up drag-and-drop support for .adl files on main window */
  mainWindowController->setDisplayWindowFactory(
      [displayPalette, &palette, fixed10Font, fixed13Font](
          std::weak_ptr<DisplayState> weakState) -> DisplayWindow * {
        return new DisplayWindow(displayPalette, palette,
            fixed10Font, fixed13Font, weakState);
      });
  mainWindowController->setDisplayWindowRegistrar(registerDisplayWindow);

  QObject::connect(newAct, &QAction::triggered, &win,
      [state, displayPalette, &win, fixed10Font, &palette, fixed13Font,
          registerDisplayWindow]() {
        if (!state->editMode) {
          return;
        }

        auto *displayWin = new DisplayWindow(displayPalette, palette,
            fixed10Font, fixed13Font, std::weak_ptr<DisplayState>(state));
        registerDisplayWindow(displayWin);
      });

  QObject::connect(openAct, &QAction::triggered, &win,
      [state, &win, displayPalette, &palette, fixed10Font, fixed13Font,
          registerDisplayWindow]() {
        static QString lastDirectory;
  QFileDialog dialog(&win, QStringLiteral("Open Display"));
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilters({
            QStringLiteral("MEDM Display Files (*.adl)"),
            QStringLiteral("All Files (*)")});
        dialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);
        dialog.setModal(true);
        dialog.setWindowModality(Qt::ApplicationModal);
        if (!lastDirectory.isEmpty()) {
          dialog.setDirectory(lastDirectory);
        }

        if (dialog.exec() != QDialog::Accepted) {
          return;
        }

        const QString selected = dialog.selectedFiles().value(0);
        if (selected.isEmpty()) {
          return;
        }

        lastDirectory = QFileInfo(selected).absolutePath();

        QString errorMessage;
        auto *displayWin = new DisplayWindow(displayPalette, palette,
            fixed10Font, fixed13Font, std::weak_ptr<DisplayState>(state));
        if (!displayWin->loadFromFile(selected, &errorMessage)) {
          const QString message = errorMessage.isEmpty()
              ? QStringLiteral("Failed to open display:\n%1").arg(selected)
              : errorMessage;
          QMessageBox::critical(&win, QStringLiteral("Open Display"),
              message);
          delete displayWin;
          return;
        }

        registerDisplayWindow(displayWin);
      });

  QObject::connect(editModeButton, &QRadioButton::toggled, &win,
      [state, updateMenus](bool checked) {
        state->editMode = checked;
        if (!checked) {
          state->createTool = CreateTool::kNone;
          for (auto &display : state->displays) {
            if (!display.isNull()) {
              display->handleEditModeChanged(checked);
              display->clearSelection();
              display->syncCreateCursor();
            }
          }
        } else {
          for (auto &display : state->displays) {
            if (!display.isNull()) {
              display->handleEditModeChanged(checked);
              display->syncCreateCursor();
            }
          }
        }
        if (updateMenus && *updateMenus) {
          (*updateMenus)();
        }
      });
  editModeButton->setChecked(true);
  modeStack->setCurrentWidget(modeButtonsWidget);
  if (options.startInExecuteMode) {
    executeModeButton->setChecked(true);
    editModeButton->setEnabled(false);
    executeModeButton->setEnabled(false);
    modeStack->setCurrentWidget(executeOnlyWidget);
  }

  if (updateMenus && *updateMenus) {
    (*updateMenus)();
  }

  panelLayout->addWidget(modeBox);

  layout->addWidget(modePanel, 0, Qt::AlignLeft);
  layout->addStretch();

  central->setLayout(layout);
  win.setCentralWidget(central);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
  if (remoteContext.active) {
    remoteContext.hostWindow = static_cast<Window>(win.winId());
    XChangeProperty(remoteContext.display, remoteContext.rootWindow,
        remoteContext.propertyAtom, XA_WINDOW, 32, PropModeReplace,
        reinterpret_cast<const unsigned char *>(&remoteContext.hostWindow), 1);
    XFlush(remoteContext.display);
    remoteContext.propertyRegistered = true;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &win,
        [remote = &remoteContext]() {
          if (remote->display && remote->propertyRegistered) {
            XDeleteProperty(remote->display, remote->rootWindow,
                remote->propertyAtom);
            XFlush(remote->display);
            remote->propertyRegistered = false;
          }
        });
    auto remoteHandler =
        [state, displayPalette, &palette, fixed10Font, fixed13Font, &win,
            registerDisplayWindow](const QString &filename,
                const QString &macroString, const QString &geometryString) {
          fprintf(stdout, "\nFile Dispatch Request:\n");
          if (!filename.isEmpty()) {
            fprintf(stdout, "  filename = %s\n",
                filename.toLocal8Bit().constData());
          }
          if (!macroString.isEmpty()) {
            fprintf(stdout, "  macro = %s\n",
                macroString.toLocal8Bit().constData());
          }
          if (!geometryString.isEmpty()) {
            fprintf(stdout, "  geometry = %s\n",
                geometryString.toLocal8Bit().constData());
          }
          fflush(stdout);

          const QString resolved = resolveDisplayFile(filename);
          if (resolved.isEmpty()) {
            fprintf(stderr, "\nCannot access file: %s\n",
                filename.toLocal8Bit().constData());
            fflush(stderr);
            return;
          }

          const MacroMap macros = parseMacroDefinitionString(macroString);
          auto *displayWin = new DisplayWindow(displayPalette, palette,
              fixed10Font, fixed13Font,
              std::weak_ptr<DisplayState>(state));
          QString errorMessage;
          if (!displayWin->loadFromFile(resolved, &errorMessage, macros)) {
            const QString message = errorMessage.isEmpty()
                ? QStringLiteral("Failed to open display:\n%1").arg(resolved)
                : errorMessage;
            QMessageBox::critical(&win, QStringLiteral("Open Display"),
                message);
            delete displayWin;
            return;
          }

          if (!geometryString.isEmpty()) {
            const auto spec = geometrySpecFromString(geometryString);
            if (spec) {
              applyCommandLineGeometry(displayWin, *spec);
            } else {
              fprintf(stderr, "\nInvalid geometry: %s\n",
                  geometryString.toLocal8Bit().constData());
              fflush(stderr);
            }
          }

          registerDisplayWindow(displayWin);
        };
    remoteFilter = std::make_unique<RemoteRequestFilter>(
        remoteContext.propertyAtom, remoteContext.hostWindow, remoteHandler);
    if (QCoreApplication *core = QCoreApplication::instance()) {
      core->installNativeEventFilter(remoteFilter.get());
    }
  }
#endif

  const MacroMap macroDefinitions = parseMacroDefinitionString(options.macroString);
  bool loadedAnyDisplay = false;
  DisplayWindow *testSaveWindow = nullptr;

  if (!options.resolvedDisplayFiles.isEmpty()) {
    for (const QString &resolved : options.resolvedDisplayFiles) {
      auto *displayWin = new DisplayWindow(displayPalette, palette,
          fixed10Font, fixed13Font, std::weak_ptr<DisplayState>(state));
      QString errorMessage;
      if (!displayWin->loadFromFile(resolved, &errorMessage, macroDefinitions)) {
        const QString message = errorMessage.isEmpty()
            ? QStringLiteral("Failed to open display:\n%1").arg(resolved)
            : errorMessage;
        QMessageBox::critical(&win, QStringLiteral("Open Display"),
            message);
        delete displayWin;
        continue;
      }
      if (geometrySpec) {
        applyCommandLineGeometry(displayWin, *geometrySpec);
      }
      registerDisplayWindow(displayWin);
      loadedAnyDisplay = true;
      if (!testSaveWindow) {
        testSaveWindow = displayWin;
      }
      if (options.testSave) {
        break;
      }
    }
  }

  if (options.testSave) {
    if (!testSaveWindow) {
      fprintf(stderr, "\nFailed to load ADL file for -testSave\n");
      fflush(stderr);
      return 1;
    }
    QPointer<DisplayWindow> target(testSaveWindow);
    QTimer::singleShot(0, &win, [target]() {
      const QString outputPath = QStringLiteral("/tmp/qtedmTest.adl");
      if (DisplayWindow *window = target.data()) {
        if (!window->saveToPath(outputPath)) {
          fprintf(stderr, "\nFailed to save display to %s\n",
              outputPath.toLocal8Bit().constData());
          fflush(stderr);
          QCoreApplication::exit(1);
          return;
        }
      } else {
        fprintf(stderr, "\nDisplay window unavailable for test save\n");
        fflush(stderr);
        QCoreApplication::exit(1);
        return;
      }
      QCoreApplication::exit(0);
    });
  }

  win.adjustSize();
  win.setFixedSize(win.sizeHint());
  const bool minimizeMainWindow =
      options.startInExecuteMode && loadedAnyDisplay;
  if (minimizeMainWindow) {
    win.showMinimized();
  } else {
    win.show();
    positionWindowTopRight(&win, kMainWindowRightMargin, kMainWindowTopMargin);
    QTimer::singleShot(0, &win,
        [&, rightMargin = kMainWindowRightMargin,
            topMargin = kMainWindowTopMargin]() {
          positionWindowTopRight(&win, rightMargin, topMargin);
        });
  }
  const int exitCode = app.exec();
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
  if (remoteFilter) {
    if (QCoreApplication *core = QCoreApplication::instance()) {
      core->removeNativeEventFilter(remoteFilter.get());
    }
    remoteFilter.reset();
  }
  if (remoteContext.display) {
    if (remoteContext.propertyRegistered) {
      XDeleteProperty(remoteContext.display, remoteContext.rootWindow,
          remoteContext.propertyAtom);
      XFlush(remoteContext.display);
      remoteContext.propertyRegistered = false;
    }
    XCloseDisplay(remoteContext.display);
    remoteContext.display = nullptr;
  }
#endif
  return exitCode;
}
