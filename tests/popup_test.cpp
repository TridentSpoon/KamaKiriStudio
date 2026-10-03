// SPDX-License-Identifier: MIT
#include "popup.h"
#include <QtTest>
class PopupTest: public QObject {
    Q_OBJECT
private slots:
    void fluentLightAndDarkRemainReadable() {
        StudioPopup light(StudioPopup::Dashboard,true,nullptr,"fluent","light");
        StudioPopup dark(StudioPopup::Dashboard,true,nullptr,"fluent","dark");
        QVERIFY(light.palette().color(QPalette::Window).lightnessF()>.8);
        QVERIFY(light.palette().color(QPalette::WindowText).lightnessF()<.2);
        QVERIFY(dark.palette().color(QPalette::Window).lightnessF()<.2);
        QVERIFY(dark.palette().color(QPalette::WindowText).lightnessF()>.8);
        light.show();QTest::qWait(30);auto image=light.grab().toImage();
        QCOMPARE(image.pixelColor(0,0).alpha(),0);QVERIFY(image.pixelColor(image.width()/2,image.height()/2).alpha()>200);
    }
};
QTEST_MAIN(PopupTest)
#include "popup_test.moc"
