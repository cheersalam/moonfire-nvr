// This file is part of Moonfire NVR, a security camera network video recorder.
// Copyright (C) 2016 Scott Lamb <slamb@slamb.org>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations including
// the two.
//
// You must obey the GNU General Public License in all respects for all
// of the code used other than OpenSSL. If you modify file(s) with this
// exception, you may extend this exception to your version of the
// file(s), but you are not obligated to do so. If you do not wish to do
// so, delete this exception statement from your version. If you delete
// this exception statement from all source files in the program, then
// also delete it here.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// sqlite-test.cc: tests of the sqlite.h interface.

#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "sqlite.h"
#include "string.h"
#include "testutil.h"

DECLARE_bool(alsologtostderr);

namespace moonfire_nvr {
namespace {

class SqliteTest : public testing::Test {
 protected:
  SqliteTest() {
    tmpdir_ = PrepareTempDirOrDie("sqlite-test");

    std::string error_message;
    CHECK(db_.Open(StrCat(tmpdir_, "/db").c_str(),
                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, &error_message))
        << error_message;

    std::string create_sql = ReadFileOrDie("../src/schema.sql");
    DatabaseContext ctx(&db_);
    CHECK(RunStatements(&ctx, create_sql, &error_message)) << error_message;
  }

  std::string tmpdir_;
  Database db_;
};

TEST_F(SqliteTest, JustCreate) {}

TEST_F(SqliteTest, BindAndColumn) {
  std::string error_message;
  auto insert_stmt = db_.Prepare(
      "insert into camera (uuid,  short_name,  retain_bytes) "
      "            values (:uuid, :short_name, :retain_bytes)",
      nullptr, &error_message);
  ASSERT_TRUE(insert_stmt.valid()) << error_message;
  const char kBlob[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  re2::StringPiece blob_piece = re2::StringPiece(kBlob, sizeof(kBlob));
  const char kText[] = "foo";
  const int64_t kInt64 = INT64_C(0xbeeffeedface);

  DatabaseContext ctx(&db_);
  {
    auto run = ctx.Borrow(&insert_stmt);
    run.BindBlob(1, blob_piece);
    run.BindText(2, kText);
    run.BindInt64(3, kInt64);
    ASSERT_EQ(SQLITE_DONE, run.Step()) << run.error_message();
  }

  {
    auto run = ctx.UseOnce("select uuid, short_name, retain_bytes from camera");
    ASSERT_EQ(SQLITE_ROW, run.Step()) << run.error_message();
    EXPECT_EQ(ToHex(blob_piece, true), ToHex(run.ColumnBlob(0), true));
    EXPECT_EQ(kText, run.ColumnText(1).as_string());
    EXPECT_EQ(kInt64, run.ColumnInt64(2));
    ASSERT_EQ(SQLITE_DONE, run.Step()) << run.error_message();
  }
}

}  // namespace
}  // namespace moonfire_nvr

int main(int argc, char **argv) {
  FLAGS_alsologtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}
