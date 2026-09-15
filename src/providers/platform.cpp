#include <QCoreApplication>
#include <QNetworkInformation>
#include <QTimer>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

#include "platform.h"


void Platform::init()
{
    // Listen to network reachability
    QNetworkInformation::loadDefaultBackend();
    auto networkInfo = QNetworkInformation::instance();
    this->setOnline(networkInfo->reachability() == QNetworkInformation::Reachability::Online);
    this->setIsEthernet(networkInfo->transportMedium() == QNetworkInformation::TransportMedium::Ethernet);

    QObject::connect(networkInfo, &QNetworkInformation::reachabilityChanged, this, [this](auto reachability ) {
        this->setOnline(reachability == QNetworkInformation::Reachability::Online);
    });

    QObject::connect(networkInfo, &QNetworkInformation::transportMediumChanged, this, [this](auto transportMedium ) {
        this->setIsEthernet(transportMedium == QNetworkInformation::TransportMedium::Ethernet);
    });

#ifdef Q_OS_ANDROID
    // Fallback: on some devices the Android backend of QNetworkInformation never leaves
    // Reachability::Unknown (seen on Android 7.1 / API 25 with Qt 6.5.3, WiFi connected and
    // validated), so the network icon shows "offline" forever. Ask ConnectivityManager directly
    // in that case and re-check every 10 seconds (one JNI call, negligible cost).
    if (networkInfo->reachability() == QNetworkInformation::Reachability::Unknown) {
        auto pollAndroidNetworkState = [this]() {
            QJniObject context = QNativeInterface::QAndroidApplication::context();
            jint state = QJniObject::callStaticMethod<jint>("hr/envizia/letihomeplus/LetiHomePlus", "networkState",
                                                            "(Landroid/content/Context;)I", context.object());
            this->setOnline(state != 0);
            this->setIsEthernet(state == 2);
        };
        pollAndroidNetworkState();
        auto *timer = new QTimer(this);
        timer->setInterval(10000);
        QObject::connect(timer, &QTimer::timeout, this, pollAndroidNetworkState);
        timer->start();
    }
#endif

    this->setIsTelevision(this->isTelevision());
}

// maybe we will support some other platform in the future
#ifdef Q_OS_ANDROID
#include <QJniObject>

/*
 these are JNI functions called from java
*/

void onPackagesChanged(JNIEnv *env , jobject /* self */, jstring jaction, jstring jpackagename, jstring jappname)
{
    const char *action = env->GetStringUTFChars(jaction, nullptr);
    QString qaction = QString::fromUtf8(action);
    env->ReleaseStringUTFChars(jaction, action);

    const char *packagename = env->GetStringUTFChars(jpackagename, nullptr);
    QString qpackagename = QString::fromUtf8(packagename);
    env->ReleaseStringUTFChars(jpackagename, packagename);

    const char *appname = env->GetStringUTFChars(jappname, nullptr);
    QString qappname = QString::fromUtf8(appname);
    env->ReleaseStringUTFChars(jappname, appname);

    QMetaObject::invokeMethod(&Platform::instance(), "packagesChanged", Qt::QueuedConnection,
                              Q_ARG(QString, qaction),
                              Q_ARG(QString, qpackagename),
                              Q_ARG(QString, qappname));
}

// called on JNI LOAD, register native methods to corresponding classes
jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
{
    JNIEnv* env;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    // get main receiver class
    QJniObject receiver = QJniObject("hr/envizia/letihomeplus/PackagesChangedReceiver");
    jclass receiverClass = env->GetObjectClass(receiver.object<jobject>());
    if (!receiverClass)
    {
        // this should never happen
        qWarning() << "receiver class not found!";
        return JNI_ERR;
    }

    // register native methods
    JNINativeMethod packagesMethods[] {{ "onPackagesChanged", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void *>(onPackagesChanged) }};
    env->RegisterNatives(receiverClass, packagesMethods, sizeof(packagesMethods) / sizeof(packagesMethods[0]));
    env->DeleteLocalRef(receiverClass);

    return JNI_VERSION_1_6;
}
#endif

// get application list from java and convert to QVariantList format that can be used directly in QML
QVariantList Platform::applicationList()
{
    QVariantList appList;

#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();

    QJniObject applications = activity.callObjectMethod("applicationList", "()Ljava/util/Map;");
    auto entriesSet = applications.callObjectMethod("entrySet", "()Ljava/util/Set;");
    auto entriesSetIterator = entriesSet.callObjectMethod("iterator", "()Ljava/util/Iterator;");

    while (entriesSetIterator.callMethod<jboolean>("hasNext"))
    {
        auto entry = entriesSetIterator.callObjectMethod("next", "()Ljava/lang/Object;");
        auto packageName = entry.callObjectMethod("getKey", "()Ljava/lang/Object;").toString();
        auto applicationName = entry.callObjectMethod("getValue", "()Ljava/lang/Object;").toString();

        QVariantMap data;
        data["packageName"] = packageName;
        data["applicationName"] = applicationName;
        appList.append(data);
    }

    // Sort the appList by "applicationName" (case-sensitive)
    std::sort(appList.begin(), appList.end(), [](const QVariant &a, const QVariant &b) {
        QString nameA = a.toMap().value("applicationName").toString();
        QString nameB = b.toMap().value("applicationName").toString();
        return nameA < nameB; // Case-sensitive comparison
    });

#else
    for (int i = 0; i < 20; i++)
    {
        QVariantMap data;
        data["packageName"] = (i == 0 ? "hr.envizia.letihomeplus" : "hr.test.home" + QString::number(i));
        data["applicationName"] = "App " + QString::number(i);
        appList.append(data);
    }
#endif

    return appList;
}

// open application by package name
void Platform::openApplication(const QString &packageName)
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>(
        "openApplication",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(packageName).object<jstring>());

#else

    Q_UNUSED(packageName);

#endif
}

// open wallpaper picker menu
void Platform::pickWallpaper()
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>("pickWallpaper");

#endif
}

// return if system clock is in 24 hour format
bool Platform::is24HourFormat()
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    return activity.callMethod<jboolean>("is24HourFormat");

#endif

    return true;
}

// return if Android TV OS device
bool Platform::isTelevision()
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    return activity.callMethod<jboolean>("isTelevision");

#endif

    return false;
}

void Platform::openSettings()
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>("openSettings");

#endif
}

void Platform::openNetworkSettings(bool isEthernet)
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>("openNetworkSettings", "(Z)V", static_cast<jboolean>(isEthernet));

#else
    Q_UNUSED(isEthernet);
#endif
}

void Platform::openAppInfo(const QString &packageName)
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>(
        "openAppInfo",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(packageName).object<jstring>());

#endif
}

void Platform::openLetiHomePage()
{
#ifdef Q_OS_ANDROID

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>("openLetiHomePage");

#endif
}

QVariantList Platform::tvInputs()
{
    QVariantList inputList;

#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject inputsMap = activity.callObjectMethod("getTvInputs", "()Ljava/util/Map;");
    auto entriesSet = inputsMap.callObjectMethod("entrySet", "()Ljava/util/Set;");
    auto entriesSetIterator = entriesSet.callObjectMethod("iterator", "()Ljava/util/Iterator;");

    while (entriesSetIterator.callMethod<jboolean>("hasNext"))
    {
        auto entry = entriesSetIterator.callObjectMethod("next", "()Ljava/lang/Object;");
        auto inputId = entry.callObjectMethod("getKey", "()Ljava/lang/Object;").toString();
        auto inputLabel = entry.callObjectMethod("getValue", "()Ljava/lang/Object;").toString();

        QVariantMap data;
        data["inputId"] = inputId;
        data["inputLabel"] = inputLabel;
        inputList.append(data);
    }
#endif

    return inputList;
}

void Platform::setTvInput(const QString &inputId)
{
#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    activity.callMethod<void>(
        "setInput",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(inputId).object<jstring>());
#else
    Q_UNUSED(inputId);
#endif
}
