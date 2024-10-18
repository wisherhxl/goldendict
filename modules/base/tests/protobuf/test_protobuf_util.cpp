/*
 * Copyright (c) 2020-2022, Shanghai Institute of Laser Development Team
 *
 * SPDX-License-Identifier: MIT License
 *
 * Change Logs:
 * Date           Author           Notes
 * 2023-08-21     Huang Xiling     first version
 */

#include <google/protobuf/util/message_differencer.h>
#include <QTest>
#include <random>
#include <string>

#include "proto/base.pb.h"
#include "tiger/base.hpp"

class TestProtobufUtil : public QObject {
    Q_OBJECT
   private slots:
    void testProtobufToJson();
    void testProtobufToBin();

   private:
    void setRandomBaseTest(ti::BaseTest* src);
};

void TestProtobufUtil::testProtobufToJson() {
    ti::BaseTest src, dst;
    setRandomBaseTest(&src);
    const auto url = QDir::currentPath().append("/test/bt_test.json");
    const auto r_write = ti::ProtobufUtil::writeMessageToJson(src, url);
    QVERIFY(r_write);
    const auto r_read = ti::ProtobufUtil::readMessageFromJson(&dst, url);
    QVERIFY(r_read);
    const auto r_cmp = google::protobuf::util::MessageDifferencer::Equals(src, dst);
    QVERIFY(r_cmp);
}

void TestProtobufUtil::testProtobufToBin() {
    ti::BaseTest src, dst;
    setRandomBaseTest(&src);
    const auto url = QDir::currentPath().append("/test/bt_test.bin");
    const auto r_write = ti::ProtobufUtil::writeMessageToBin(src, url);
    QVERIFY(r_write);
    const auto r_read = ti::ProtobufUtil::readMessageFromBin(&dst, url);
    QVERIFY(r_read);
    const auto r_cmp = google::protobuf::util::MessageDifferencer::Equals(src, dst);
    QVERIFY(r_cmp);
}

void TestProtobufUtil::setRandomBaseTest(ti::BaseTest* src) {
    src->set_int_var_(ti::RandomUtil::getRandomInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    src->set_double_var_(
        ti::RandomUtil::getRandomDouble(std::numeric_limits<double>::min(), std::numeric_limits<double>::max()));
    src->set_float_var_(static_cast<float>(
        ti::RandomUtil::getRandomDouble(std::numeric_limits<float>::min(), std::numeric_limits<float>::max())));
    src->set_str_var_(ti::RandomUtil::getRandomString(256));
    src->set_bool_var_(ti::RandomUtil::getRandomInt(0, 1));
    for (int i = 0; i < 128; ++i) {
        auto* arr_cur = src->mutable_int_array_()->Add();
        *arr_cur = ti::RandomUtil::getRandomInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        auto* map_ptr = src->mutable_int_str_map_();
        (*map_ptr)[i] = ti::RandomUtil::getRandomString(8);
    }
}

QTEST_MAIN(TestProtobufUtil)
#include "test_protobuf_util.moc"