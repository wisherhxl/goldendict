#include <QtTest/QtTest>

#include "tiger/test.pb.h"
#include "tiger/ti_base.tp.h"

class TestProtoMessage : public QObject {
    Q_OBJECT

private slots:
    void buildsExpectedMessage();
};

void TestProtoMessage::buildsExpectedMessage() {
    tiger::TestRequest request;
    request.set_id("z_example");
    request.set_name("Tiger Qt Example");
    request.add_tags("module");
    request.add_tags("proto");

    QCOMPARE(QString::fromStdString(request.id()), QString("z_example"));
    QCOMPARE(QString::fromStdString(request.name()), QString("Tiger Qt Example"));
    QCOMPARE(request.tags_size(), 2);
    QCOMPARE(QString::fromStdString(request.tags(0)), QString("module"));
    QCOMPARE(QString::fromStdString(request.tags(1)), QString("proto"));
    QCOMPARE(tiWrap(request.tags_size(), 0, 9), 2);
    QVERIFY(request.GetDescriptor() != nullptr);
    QVERIFY(request.GetReflection() != nullptr);
    QVERIFY(!QString::fromStdString(request.ShortDebugString()).isEmpty());
}

QTEST_MAIN(TestProtoMessage)

#include "test_proto_message.moc"
