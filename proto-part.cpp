

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "proto.pb.h"

#include <google/protobuf/util/message_differencer.h>

using namespace ::testing;
using namespace google::protobuf::util;

TEST(First, Test)
{
  tutorial::AddressBook ab;
}


struct membuf: std::streambuf {
    membuf(char const* base, size_t size) {
        char* p(const_cast<char*>(base));
        this->setg(p, p, p + size);
    }
};

struct imemstream: virtual membuf, std::istream {
    imemstream(char const* base, size_t size)
        : membuf(base, size)
        , std::istream(static_cast<std::streambuf*>(this)) {
    }
};

TEST(Enc, Test)
{
  tutorial::Person pm;
  {
    // create person pm;
    tutorial::Person::PhoneNumber ph;

    auto* ph_p = pm.add_phones();

    *ph_p = ph;

    pm.set_name("Jerome");
    pm.set_email("jbode@example.net");
  } 
  // harden into const ref;
  const tutorial::Person& p = pm; 

  ASSERT_THAT(p.name(), StrEq("Jerome"));

  // serialize to string
  std::string p_str;
  ASSERT_TRUE(p.SerializeToString(&p_str));
  ASSERT_TRUE(p_str.size() > 0u);

  // convert into istream
  const char* p_cstr = p_str.c_str();
  const std::size_t p_cstr_len = p_str.size();

  imemstream p_stream(p_cstr, p_cstr_len);

  // read istream into j
  tutorial::Person j;

  ASSERT_TRUE(j.ParseFromIstream(&p_stream));

  // test reading worked
  ASSERT_THAT(j.name(), StrEq("Jerome"));
  ASSERT_TRUE(MessageDifferencer::Equals(j, p));

  std::cerr << '¬';
  std::cerr << p_str;
  std::cerr << '¬';
}
