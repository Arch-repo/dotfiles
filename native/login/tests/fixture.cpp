#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardItemModel>
#include <QTest>
#include <QDir>
#include <QDebug>

class Settings : public QObject {
    Q_OBJECT
public:
    QMap<QString, QString> values;
    Q_INVOKABLE QString stringValue(const QString &key) const { return values.value(key); }
    Q_INVOKABLE bool boolValue(const QString &key) const { return values.value(key) == "true"; }
};
class Model : public QStandardItemModel {
    Q_OBJECT
    Q_PROPERTY(int lastIndex READ lastIndex CONSTANT)
    Q_PROPERTY(int count READ rowCount NOTIFY rowsChanged)
public:
    using QStandardItemModel::QStandardItemModel;
    int lastIndex() const { return 0; }
    QHash<int,QByteArray> roleNames() const override { return {{Qt::DisplayRole, "name"}}; }
signals:
    void rowsChanged();
};
class Keyboard : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool capsLock MEMBER capsLock NOTIFY changed)
    Q_PROPERTY(int currentLayout MEMBER currentLayout NOTIFY changed)
    Q_PROPERTY(QVariantList layouts MEMBER layouts CONSTANT)
public:
    bool capsLock=false;
    int currentLayout=0;
    QVariantList layouts={QVariantMap{{"longName", "Italiano"}},QVariantMap{{"longName", "English"}}};
signals:
    void changed();
};
class Proxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(bool canSuspend READ yes CONSTANT)
    Q_PROPERTY(bool canReboot READ yes CONSTANT)
    Q_PROPERTY(bool canPowerOff READ yes CONSTANT)
public:
    int calls=0, session=-1;
    QString user;
    bool nonempty=false;
    QString hostName() const { return "desktop"; }
    bool yes() const { return true; }
    Q_INVOKABLE void login(const QString &name, const QString &password, int index) { ++calls; user=name; session=index; nonempty=!password.isEmpty(); }
    Q_INVOKABLE void suspend() {}
    Q_INVOKABLE void reboot() {}
    Q_INVOKABLE void powerOff() {}
signals:
    void informationMessage(const QString &message);
    void loginFailed();
    void loginSucceeded();
};
static void require(bool ok, const char *message) { if(!ok) qFatal("%s",message); }
int main(int argc,char **argv) {
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString &text){fprintf(stderr,"%s\n",text.toUtf8().constData());});
    QGuiApplication app(argc,argv);
    require(argc==3,"usage: fixture theme screenshot-directory");
    Settings config; Keyboard keyboard; Proxy proxy; Model users,sessions;
    users.appendRow(new QStandardItem("demo")); sessions.appendRow(new QStandardItem("Hyprland")); sessions.appendRow(new QStandardItem("Plasma"));
    config.values={{"Locale","it_IT"},{"AllowEmptyPassword","false"},{"Background",QUrl::fromLocalFile(QDir(argv[2]).filePath("background.png")).toString()}};
    QQuickView view;
    auto *ctx=view.rootContext();
    ctx->setContextProperty("config",&config);ctx->setContextProperty("keyboard",&keyboard);
    ctx->setContextProperty("sddm",&proxy);ctx->setContextProperty("userModel",&users);ctx->setContextProperty("sessionModel",&sessions);
    int warnings=0;
    QObject::connect(view.engine(),&QQmlEngine::warnings,[&](const QList<QQmlError>& errors){warnings+=errors.size();for(const auto &e:errors)qWarning()<<e;});
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl::fromLocalFile(QDir(argv[1]).filePath("Main.qml")));
    require(view.status()==QQuickView::Ready,"QML login failed to load");
    view.show();view.requestActivate();QTest::qWait(150);
    auto *form=view.rootObject()->findChild<QObject*>("loginForm");
    auto *password=view.rootObject()->findChild<QQuickItem*>("passwordInput");
    auto *picker=view.rootObject()->findChild<QObject*>("sessionPicker");
    require(form && password && picker,"Missing interactive login controls");
    require(password->hasActiveFocus(),"The initial keyboard focus is not on the password");
    require(QMetaObject::invokeMethod(form,"submit"),"Cannot submit");require(proxy.calls==0,"Empty password bypassed the configured policy");
    picker->setProperty("currentIndex",1);password->setProperty("text","synthetic-fixture");password->forceActiveFocus();
    QTest::keyClick(&view,Qt::Key_Return);QTest::qWait(30);
    require(proxy.calls==1 && proxy.session==1 && proxy.user=="demo" && proxy.nonempty,"Enter did not send the selected user/session");
    QMetaObject::invokeMethod(form,"submit");require(proxy.calls==1,"Duplicate authentication while pending");
    emit proxy.informationMessage("Appoggia il dito sul lettore");
    require(form->property("message").toString().contains("dito"),"PAM fingerprint prompt was hidden");
    emit proxy.loginFailed();QTest::qWait(30);
    require(!form->property("pending").toBool() && password->property("text").toString().isEmpty() && password->hasActiveFocus(),"Failure must clear password and restore focus");
    config.values["AllowEmptyPassword"]="true";
    QMetaObject::invokeMethod(form,"submit");require(proxy.calls==2 && !proxy.nonempty,"Fingerprint PAM initiation was blocked");
    emit proxy.loginFailed();form->setProperty("message","");
    for(const QSize size:{QSize(1280,800),QSize(1920,1080),QSize(800,600),QSize(480,800)}) {
        view.resize(size);QTest::qWait(120);
        const QPointF point=password->mapToItem(view.rootObject(),QPointF(0,0));
        require(point.x()>=0 && point.x()+password->width()<=size.width()+1,"Password cropped horizontally");
        require(point.y()>=0 && point.y()+password->height()<=size.height()+1,"Password cropped vertically");
        require(view.grabWindow().save(QDir(argv[2]).filePath(QString("login-%1x%2.png").arg(size.width()).arg(size.height()))),"Screenshot failed");
    }
    require(warnings==0,"QML warnings/errors in the login theme");
    qInfo()<<"Qt6 login: Enter, user/session routing, duplicate suppression, failure focus, fingerprint initiation and four layouts verified";
}
#include "fixture.moc"
