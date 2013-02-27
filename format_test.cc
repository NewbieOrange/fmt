/*
 Formatting library tests.

 Copyright (c) 2012, Victor Zverovich
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cctype>
#include <cfloat>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <iomanip>
#include <memory>
#include <gtest/gtest.h>
#include "format.h"

#include <stdint.h>

using std::size_t;

using fmt::internal::Array;
using fmt::BasicWriter;
using fmt::Formatter;
using fmt::Format;
using fmt::FormatError;
using fmt::StringRef;
using fmt::Writer;
using fmt::pad;

#define FORMAT_TEST_THROW_(statement, expected_exception, message, fail) \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_ \
  if (::testing::internal::ConstCharPtr gtest_msg = "") { \
    bool gtest_caught_expected = false; \
    try { \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement); \
    } \
    catch (expected_exception const& e) { \
      gtest_caught_expected = true; \
      if (std::strcmp(message, e.what()) != 0) \
        throw; \
    } \
    catch (...) { \
      gtest_msg.value = \
          "Expected: " #statement " throws an exception of type " \
          #expected_exception ".\n  Actual: it throws a different type."; \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__); \
    } \
    if (!gtest_caught_expected) { \
      gtest_msg.value = \
          "Expected: " #statement " throws an exception of type " \
          #expected_exception ".\n  Actual: it throws nothing."; \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__); \
    } \
  } else \
    GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__): \
      fail(gtest_msg.value)

#define EXPECT_THROW_MSG(statement, expected_exception, expected_message) \
  FORMAT_TEST_THROW_(statement, expected_exception, expected_message, \
      GTEST_NONFATAL_FAILURE_)

// Increment a number in a string.
void Increment(char *s) {
  for (int i = static_cast<int>(std::strlen(s)) - 1; i >= 0; --i) {
    if (s[i] != '9') {
      ++s[i];
      break;
    }
    s[i] = '0';
  }
}

enum {BUFFER_SIZE = 256};

#ifdef _MSC_VER
# define vsnprintf _vsprintf_s
#endif

void SPrintf(char *buffer, const char *format, ...) {
  std::va_list args;
  va_start(args, format);
  vsnprintf(buffer, BUFFER_SIZE, format, args);
  va_end(args);
}

TEST(UtilTest, Increment) {
  char s[10] = "123";
  Increment(s);
  EXPECT_STREQ("124", s);
  s[2] = '8';
  Increment(s);
  EXPECT_STREQ("129", s);
  Increment(s);
  EXPECT_STREQ("130", s);
  EXPECT_STREQ("130", s);
  s[1] = s[2] = '9';
  Increment(s);
  EXPECT_STREQ("200", s);
}

class TestString {
 private:
  std::string value_;

 public:
  explicit TestString(const char *value = "") : value_(value) {}

  friend std::ostream &operator<<(std::ostream &os, const TestString &s) {
    os << s.value_;
    return os;
  }
};

TEST(ArrayTest, Ctor) {
  Array<char, 123> array;
  EXPECT_EQ(0u, array.size());
  EXPECT_EQ(123u, array.capacity());
}

TEST(ArrayTest, Access) {
  Array<char, 10> array;
  array[0] = 11;
  EXPECT_EQ(11, array[0]);
  array[3] = 42;
  EXPECT_EQ(42, *(&array[0] + 3));
  const Array<char, 10> &carray = array;
  EXPECT_EQ(42, carray[3]);
}

TEST(ArrayTest, Resize) {
  Array<char, 123> array;
  array[10] = 42;
  EXPECT_EQ(42, array[10]);
  array.resize(20);
  EXPECT_EQ(20u, array.size());
  EXPECT_EQ(123u, array.capacity());
  EXPECT_EQ(42, array[10]);
  array.resize(5);
  EXPECT_EQ(5u, array.size());
  EXPECT_EQ(123u, array.capacity());
  EXPECT_EQ(42, array[10]);
}

TEST(ArrayTest, Grow) {
  Array<int, 10> array;
  array.resize(10);
  for (int i = 0; i < 10; ++i)
    array[i] = i * i;
  array.resize(20);
  EXPECT_EQ(20u, array.size());
  EXPECT_EQ(20u, array.capacity());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i * i, array[i]);
}

TEST(ArrayTest, Clear) {
  Array<char, 10> array;
  array.resize(20);
  array.clear();
  EXPECT_EQ(0u, array.size());
  EXPECT_EQ(20u, array.capacity());
}

TEST(ArrayTest, PushBack) {
  Array<int, 10> array;
  array.push_back(11);
  EXPECT_EQ(11, array[0]);
  EXPECT_EQ(1u, array.size());
  array.resize(10);
  array.push_back(22);
  EXPECT_EQ(22, array[10]);
  EXPECT_EQ(11u, array.size());
  EXPECT_EQ(15u, array.capacity());
}

TEST(ArrayTest, Append) {
  Array<char, 10> array;
  const char *test = "test";
  array.append(test, test + 5);
  EXPECT_STREQ("test", &array[0]);
  EXPECT_EQ(5u, array.size());
  array.resize(10);
  array.append(test, test + 2);
  EXPECT_EQ('t', array[10]);
  EXPECT_EQ('e', array[11]);
  EXPECT_EQ(12u, array.size());
  EXPECT_EQ(15u, array.capacity());
}

TEST(WriterTest, WriteInt) {
  EXPECT_EQ("42", str(Writer() << 42));
  EXPECT_EQ("-42", str(Writer() << -42));
}

TEST(WriterTest, oct) {
  using fmt::oct;
  EXPECT_EQ("12", str(Writer() << oct(static_cast<short>(012))));
  EXPECT_EQ("12", str(Writer() << oct(012)));
  EXPECT_EQ("34", str(Writer() << oct(034u)));
  EXPECT_EQ("56", str(Writer() << oct(056l)));
  EXPECT_EQ("70", str(Writer() << oct(070ul)));
}

TEST(WriterTest, hex) {
  using fmt::hex;
  fmt::IntFormatter<int, fmt::TypeSpec<'x'> > (*phex)(int value) = hex;
  phex(42);
  // This shouldn't compile:
  //fmt::IntFormatter<short, fmt::TypeSpec<'x'> > (*phex2)(short value) = hex;

  EXPECT_EQ("cafe", str(Writer() << hex(0xcafe)));
  EXPECT_EQ("babe", str(Writer() << hex(0xbabeu)));
  EXPECT_EQ("dead", str(Writer() << hex(0xdeadl)));
  EXPECT_EQ("beef", str(Writer() << hex(0xbeeful)));
}

TEST(WriterTest, hexu) {
  using fmt::hexu;
  EXPECT_EQ("CAFE", str(Writer() << hexu(0xcafe)));
  EXPECT_EQ("BABE", str(Writer() << hexu(0xbabeu)));
  EXPECT_EQ("DEAD", str(Writer() << hexu(0xdeadl)));
  EXPECT_EQ("BEEF", str(Writer() << hexu(0xbeeful)));
}

TEST(WriterTest, WriteDouble) {
  EXPECT_EQ("4.2", str(Writer() << 4.2));
  EXPECT_EQ("-4.2", str(Writer() << -4.2));
}

class Date {
  int year_, month_, day_;
 public:
  Date(int year, int month, int day) : year_(year), month_(month), day_(day) {}

  int year() const { return year_; }
  int month() const { return month_; }
  int day() const { return day_; }

  friend std::ostream &operator<<(std::ostream &os, const Date &d) {
    os << d.year_ << '-' << d.month_ << '-' << d.day_;
    return os;
  }

  template <typename Char>
  friend BasicWriter<Char> &operator<<(BasicWriter<Char> &f, const Date &d) {
    return f << d.year_ << '-' << d.month_ << '-' << d.day_;
  }
};

class ISO8601DateFormatter {
 const Date *date_;

public:
  ISO8601DateFormatter(const Date &d) : date_(&d) {}

  template <typename Char>
  friend BasicWriter<Char> &operator<<(
      BasicWriter<Char> &w, const ISO8601DateFormatter &d) {
    return w << pad(d.date_->year(), 4, '0') << '-'
        << pad(d.date_->month(), 2, '0') << '-' << pad(d.date_->day(), 2, '0');
  }
};

ISO8601DateFormatter iso8601(const Date &d) { return ISO8601DateFormatter(d); }

TEST(WriterTest, pad) {
  using fmt::hex;
  EXPECT_EQ("    cafe", str(Writer() << pad(hex(0xcafe), 8)));
  EXPECT_EQ("    babe", str(Writer() << pad(hex(0xbabeu), 8)));
  EXPECT_EQ("    dead", str(Writer() << pad(hex(0xdeadl), 8)));
  EXPECT_EQ("    beef", str(Writer() << pad(hex(0xbeeful), 8)));

  EXPECT_EQ("     11", str(Writer() << pad(11, 7)));
  EXPECT_EQ("     22", str(Writer() << pad(22u, 7)));
  EXPECT_EQ("     33", str(Writer() << pad(33l, 7)));
  EXPECT_EQ("     44", str(Writer() << pad(44lu, 7)));

  BasicWriter<char> f;
  f.Clear();
  f << pad(42, 5, '0');
  EXPECT_EQ("00042", f.str());
  f.Clear();
  f << Date(2012, 12, 9);
  EXPECT_EQ("2012-12-9", f.str());
  f.Clear();
  f << iso8601(Date(2012, 1, 9));
  EXPECT_EQ("2012-01-09", f.str());
}

TEST(WriterTest, NoConflictWithIOManip) {
  using namespace std;
  using namespace fmt;
  EXPECT_EQ("cafe", str(Writer() << hex(0xcafe)));
  EXPECT_EQ("12", str(Writer() << oct(012)));
}

TEST(WriterTest, WWriter) {
  EXPECT_EQ(L"cafe", str(fmt::WWriter() << fmt::hex(0xcafe)));
}

TEST(FormatterTest, Escape) {
  EXPECT_EQ("{", str(Format("{{")));
  EXPECT_EQ("before {", str(Format("before {{")));
  EXPECT_EQ("{ after", str(Format("{{ after")));
  EXPECT_EQ("before { after", str(Format("before {{ after")));

  EXPECT_EQ("}", str(Format("}}")));
  EXPECT_EQ("before }", str(Format("before }}")));
  EXPECT_EQ("} after", str(Format("}} after")));
  EXPECT_EQ("before } after", str(Format("before }} after")));

  EXPECT_EQ("{}", str(Format("{{}}")));
  EXPECT_EQ("{42}", str(Format("{{{0}}}") << 42));
}

TEST(FormatterTest, UnmatchedBraces) {
  EXPECT_THROW_MSG(Format("{"), FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("}"), FormatError, "unmatched '}' in format");
  EXPECT_THROW_MSG(Format("{0{}"), FormatError, "unmatched '{' in format");
}

TEST(FormatterTest, NoArgs) {
  EXPECT_EQ("test", str(Format("test")));
}

TEST(FormatterTest, ArgsInDifferentPositions) {
  EXPECT_EQ("42", str(Format("{0}") << 42));
  EXPECT_EQ("before 42", str(Format("before {0}") << 42));
  EXPECT_EQ("42 after", str(Format("{0} after") << 42));
  EXPECT_EQ("before 42 after", str(Format("before {0} after") << 42));
  EXPECT_EQ("answer = 42", str(Format("{0} = {1}") << "answer" << 42));
  EXPECT_EQ("42 is the answer",
      str(Format("{1} is the {0}") << "answer" << 42));
  EXPECT_EQ("abracadabra", str(Format("{0}{1}{0}") << "abra" << "cad"));
}

TEST(FormatterTest, ArgErrors) {
  EXPECT_THROW_MSG(Format("{"), FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{x}"), FormatError,
      "invalid argument index in format string");
  EXPECT_THROW_MSG(Format("{0"), FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0}"), FormatError,
      "argument index is out of range in format");

  char format[BUFFER_SIZE];
  SPrintf(format, "{%u", UINT_MAX);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  SPrintf(format, "{%u}", UINT_MAX);
  EXPECT_THROW_MSG(Format(format), FormatError,
      "argument index is out of range in format");

  SPrintf(format, "{%u", UINT_MAX);
  Increment(format + 1);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  std::size_t size = std::strlen(format);
  format[size] = '}';
  format[size + 1] = 0;
  EXPECT_THROW_MSG(Format(format), FormatError, "number is too big in format");
}

TEST(FormatterTest, AutoArgIndex) {
  EXPECT_EQ("abc", str(Format("{}{}{}") << 'a' << 'b' << 'c'));
  EXPECT_THROW_MSG(Format("{0}{}") << 'a' << 'b',
      FormatError, "cannot switch from manual to automatic argument indexing");
  EXPECT_THROW_MSG(Format("{}{0}") << 'a' << 'b',
      FormatError, "cannot switch from automatic to manual argument indexing");
  EXPECT_EQ("1.2", str(Format("{:.{}}") << 1.2345 << 2));
  EXPECT_THROW_MSG(Format("{0}:.{}") << 1.2345 << 2,
      FormatError, "cannot switch from manual to automatic argument indexing");
  EXPECT_THROW_MSG(Format("{:.{0}}") << 1.2345 << 2,
      FormatError, "cannot switch from automatic to manual argument indexing");
  EXPECT_THROW_MSG(Format("{}"), FormatError,
      "argument index is out of range in format");
}

TEST(FormatterTest, EmptySpecs) {
  EXPECT_EQ("42", str(Format("{0:}") << 42));
}

TEST(FormatterTest, LeftAlign) {
  EXPECT_EQ("42  ", str(Format("{0:<4}") << 42));
  EXPECT_EQ("42  ", str(Format("{0:<4o}") << 042));
  EXPECT_EQ("42  ", str(Format("{0:<4x}") << 0x42));
  EXPECT_EQ("-42  ", str(Format("{0:<5}") << -42));
  EXPECT_EQ("42   ", str(Format("{0:<5}") << 42u));
  EXPECT_EQ("-42  ", str(Format("{0:<5}") << -42l));
  EXPECT_EQ("42   ", str(Format("{0:<5}") << 42ul));
  EXPECT_EQ("-42  ", str(Format("{0:<5}") << -42.0));
  EXPECT_EQ("-42  ", str(Format("{0:<5}") << -42.0l));
  EXPECT_EQ("c    ", str(Format("{0:<5}") << 'c'));
  EXPECT_EQ("abc  ", str(Format("{0:<5}") << "abc"));
  EXPECT_EQ("0xface  ",
      str(Format("{0:<8}") << reinterpret_cast<void*>(0xface)));
  EXPECT_EQ("def  ", str(Format("{0:<5}") << TestString("def")));
}

TEST(FormatterTest, RightAlign) {
  EXPECT_EQ("  42", str(Format("{0:>4}") << 42));
  EXPECT_EQ("  42", str(Format("{0:>4o}") << 042));
  EXPECT_EQ("  42", str(Format("{0:>4x}") << 0x42));
  EXPECT_EQ("  -42", str(Format("{0:>5}") << -42));
  EXPECT_EQ("   42", str(Format("{0:>5}") << 42u));
  EXPECT_EQ("  -42", str(Format("{0:>5}") << -42l));
  EXPECT_EQ("   42", str(Format("{0:>5}") << 42ul));
  EXPECT_EQ("  -42", str(Format("{0:>5}") << -42.0));
  EXPECT_EQ("  -42", str(Format("{0:>5}") << -42.0l));
  EXPECT_EQ("    c", str(Format("{0:>5}") << 'c'));
  EXPECT_EQ("  abc", str(Format("{0:>5}") << "abc"));
  EXPECT_EQ("  0xface",
      str(Format("{0:>8}") << reinterpret_cast<void*>(0xface)));
  EXPECT_EQ("  def", str(Format("{0:>5}") << TestString("def")));
}

TEST(FormatterTest, NumericAlign) {
  EXPECT_EQ("  42", str(Format("{0:=4}") << 42));
  EXPECT_EQ("+ 42", str(Format("{0:=+4}") << 42));
  EXPECT_EQ("  42", str(Format("{0:=4o}") << 042));
  EXPECT_EQ("+ 42", str(Format("{0:=+4o}") << 042));
  EXPECT_EQ("  42", str(Format("{0:=4x}") << 0x42));
  EXPECT_EQ("+ 42", str(Format("{0:=+4x}") << 0x42));
  EXPECT_EQ("-  42", str(Format("{0:=5}") << -42));
  EXPECT_EQ("   42", str(Format("{0:=5}") << 42u));
  EXPECT_EQ("-  42", str(Format("{0:=5}") << -42l));
  EXPECT_EQ("   42", str(Format("{0:=5}") << 42ul));
  EXPECT_EQ("-  42", str(Format("{0:=5}") << -42.0));
  EXPECT_EQ("-  42", str(Format("{0:=5}") << -42.0l));
  EXPECT_THROW_MSG(Format("{0:=5") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:=5}") << 'c',
      FormatError, "format specifier '=' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:=5}") << "abc",
      FormatError, "format specifier '=' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:=8}") << reinterpret_cast<void*>(0xface),
      FormatError, "format specifier '=' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:=5}") << TestString("def"),
      FormatError, "format specifier '=' requires numeric argument");
}

TEST(FormatterTest, CenterAlign) {
  EXPECT_EQ(" 42  ", str(Format("{0:^5}") << 42));
  EXPECT_EQ(" 42  ", str(Format("{0:^5o}") << 042));
  EXPECT_EQ(" 42  ", str(Format("{0:^5x}") << 0x42));
  EXPECT_EQ(" -42 ", str(Format("{0:^5}") << -42));
  EXPECT_EQ(" 42  ", str(Format("{0:^5}") << 42u));
  EXPECT_EQ(" -42 ", str(Format("{0:^5}") << -42l));
  EXPECT_EQ(" 42  ", str(Format("{0:^5}") << 42ul));
  EXPECT_EQ(" -42  ", str(Format("{0:^6}") << -42.0));
  EXPECT_EQ(" -42 ", str(Format("{0:^5}") << -42.0l));
  EXPECT_EQ("  c  ", str(Format("{0:^5}") << 'c'));
  EXPECT_EQ(" abc  ", str(Format("{0:^6}") << "abc"));
  EXPECT_EQ(" 0xface ",
      str(Format("{0:^8}") << reinterpret_cast<void*>(0xface)));
  EXPECT_EQ(" def ", str(Format("{0:^5}") << TestString("def")));
}

TEST(FormatterTest, Fill) {
  EXPECT_THROW_MSG(Format("{0:{<5}") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:{<5}}") << 'c',
      FormatError, "invalid fill character '{'");
  EXPECT_EQ("**42", str(Format("{0:*>4}") << 42));
  EXPECT_EQ("**-42", str(Format("{0:*>5}") << -42));
  EXPECT_EQ("***42", str(Format("{0:*>5}") << 42u));
  EXPECT_EQ("**-42", str(Format("{0:*>5}") << -42l));
  EXPECT_EQ("***42", str(Format("{0:*>5}") << 42ul));
  EXPECT_EQ("**-42", str(Format("{0:*>5}") << -42.0));
  EXPECT_EQ("**-42", str(Format("{0:*>5}") << -42.0l));
  EXPECT_EQ("c****", str(Format("{0:*<5}") << 'c'));
  EXPECT_EQ("abc**", str(Format("{0:*<5}") << "abc"));
  EXPECT_EQ("**0xface", str(Format("{0:*>8}")
      << reinterpret_cast<void*>(0xface)));
  EXPECT_EQ("def**", str(Format("{0:*<5}") << TestString("def")));
}

TEST(FormatterTest, PlusSign) {
  EXPECT_EQ("+42", str(Format("{0:+}") << 42));
  EXPECT_EQ("-42", str(Format("{0:+}") << -42));
  EXPECT_EQ("+42", str(Format("{0:+}") << 42));
  EXPECT_THROW_MSG(Format("{0:+}") << 42u,
      FormatError, "format specifier '+' requires signed argument");
  EXPECT_EQ("+42", str(Format("{0:+}") << 42l));
  EXPECT_THROW_MSG(Format("{0:+}") << 42ul,
      FormatError, "format specifier '+' requires signed argument");
  EXPECT_EQ("+42", str(Format("{0:+}") << 42.0));
  EXPECT_EQ("+42", str(Format("{0:+}") << 42.0l));
  EXPECT_THROW_MSG(Format("{0:+") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:+}") << 'c',
      FormatError, "format specifier '+' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:+}") << "abc",
      FormatError, "format specifier '+' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:+}") << reinterpret_cast<void*>(0x42),
      FormatError, "format specifier '+' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:+}") << TestString(),
      FormatError, "format specifier '+' requires numeric argument");
}

TEST(FormatterTest, MinusSign) {
  EXPECT_EQ("42", str(Format("{0:-}") << 42));
  EXPECT_EQ("-42", str(Format("{0:-}") << -42));
  EXPECT_EQ("42", str(Format("{0:-}") << 42));
  EXPECT_THROW_MSG(Format("{0:-}") << 42u,
      FormatError, "format specifier '-' requires signed argument");
  EXPECT_EQ("42", str(Format("{0:-}") << 42l));
  EXPECT_THROW_MSG(Format("{0:-}") << 42ul,
      FormatError, "format specifier '-' requires signed argument");
  EXPECT_EQ("42", str(Format("{0:-}") << 42.0));
  EXPECT_EQ("42", str(Format("{0:-}") << 42.0l));
  EXPECT_THROW_MSG(Format("{0:-") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:-}") << 'c',
      FormatError, "format specifier '-' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:-}") << "abc",
      FormatError, "format specifier '-' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:-}") << reinterpret_cast<void*>(0x42),
      FormatError, "format specifier '-' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:-}") << TestString(),
      FormatError, "format specifier '-' requires numeric argument");
}

TEST(FormatterTest, SpaceSign) {
  EXPECT_EQ(" 42", str(Format("{0: }") << 42));
  EXPECT_EQ("-42", str(Format("{0: }") << -42));
  EXPECT_EQ(" 42", str(Format("{0: }") << 42));
  EXPECT_THROW_MSG(Format("{0: }") << 42u,
      FormatError, "format specifier ' ' requires signed argument");
  EXPECT_EQ(" 42", str(Format("{0: }") << 42l));
  EXPECT_THROW_MSG(Format("{0: }") << 42ul,
      FormatError, "format specifier ' ' requires signed argument");
  EXPECT_EQ(" 42", str(Format("{0: }") << 42.0));
  EXPECT_EQ(" 42", str(Format("{0: }") << 42.0l));
  EXPECT_THROW_MSG(Format("{0: ") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0: }") << 'c',
      FormatError, "format specifier ' ' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0: }") << "abc",
      FormatError, "format specifier ' ' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0: }") << reinterpret_cast<void*>(0x42),
      FormatError, "format specifier ' ' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0: }") << TestString(),
      FormatError, "format specifier ' ' requires numeric argument");
}

TEST(FormatterTest, HashFlag) {
  EXPECT_EQ("42", str(Format("{0:#}") << 42));
  EXPECT_EQ("-42", str(Format("{0:#}") << -42));
  EXPECT_EQ("0x42", str(Format("{0:#x}") << 0x42));
  EXPECT_EQ("-0x42", str(Format("{0:#x}") << -0x42));
  EXPECT_EQ("042", str(Format("{0:#o}") << 042));
  EXPECT_EQ("-042", str(Format("{0:#o}") << -042));
  EXPECT_EQ("42", str(Format("{0:#}") << 42u));
  EXPECT_EQ("0x42", str(Format("{0:#x}") << 0x42u));
  EXPECT_EQ("042", str(Format("{0:#o}") << 042u));
  EXPECT_EQ("-42", str(Format("{0:#}") << -42l));
  EXPECT_EQ("0x42", str(Format("{0:#x}") << 0x42l));
  EXPECT_EQ("-0x42", str(Format("{0:#x}") << -0x42l));
  EXPECT_EQ("042", str(Format("{0:#o}") << 042l));
  EXPECT_EQ("-042", str(Format("{0:#o}") << -042l));
  EXPECT_EQ("42", str(Format("{0:#}") << 42ul));
  EXPECT_EQ("0x42", str(Format("{0:#x}") << 0x42ul));
  EXPECT_EQ("042", str(Format("{0:#o}") << 042ul));
  EXPECT_EQ("-42.0000", str(Format("{0:#}") << -42.0));
  EXPECT_EQ("-42.0000", str(Format("{0:#}") << -42.0l));
  EXPECT_THROW_MSG(Format("{0:#") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:#}") << 'c',
      FormatError, "format specifier '#' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:#}") << "abc",
      FormatError, "format specifier '#' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:#}") << reinterpret_cast<void*>(0x42),
      FormatError, "format specifier '#' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:#}") << TestString(),
      FormatError, "format specifier '#' requires numeric argument");
}

TEST(FormatterTest, ZeroFlag) {
  EXPECT_EQ("42", str(Format("{0:0}") << 42));
  EXPECT_EQ("-0042", str(Format("{0:05}") << -42));
  EXPECT_EQ("00042", str(Format("{0:05}") << 42u));
  EXPECT_EQ("-0042", str(Format("{0:05}") << -42l));
  EXPECT_EQ("00042", str(Format("{0:05}") << 42ul));
  EXPECT_EQ("-0042", str(Format("{0:05}") << -42.0));
  EXPECT_EQ("-0042", str(Format("{0:05}") << -42.0l));
  EXPECT_THROW_MSG(Format("{0:0") << 'c',
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:05}") << 'c',
      FormatError, "format specifier '0' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:05}") << "abc",
      FormatError, "format specifier '0' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:05}") << reinterpret_cast<void*>(0x42),
      FormatError, "format specifier '0' requires numeric argument");
  EXPECT_THROW_MSG(Format("{0:05}") << TestString(),
      FormatError, "format specifier '0' requires numeric argument");
}

TEST(FormatterTest, Width) {
  char format[BUFFER_SIZE];
  SPrintf(format, "{0:%u", UINT_MAX);
  Increment(format + 3);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  std::size_t size = std::strlen(format);
  format[size] = '}';
  format[size + 1] = 0;
  EXPECT_THROW_MSG(Format(format) << 0,
      FormatError, "number is too big in format");

  SPrintf(format, "{0:%u", INT_MAX + 1u);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  SPrintf(format, "{0:%u}", INT_MAX + 1u);
  EXPECT_THROW_MSG(Format(format) << 0,
      FormatError, "number is too big in format");
  EXPECT_EQ(" -42", str(Format("{0:4}") << -42));
  EXPECT_EQ("   42", str(Format("{0:5}") << 42u));
  EXPECT_EQ("   -42", str(Format("{0:6}") << -42l));
  EXPECT_EQ("     42", str(Format("{0:7}") << 42ul));
  EXPECT_EQ("   -1.23", str(Format("{0:8}") << -1.23));
  EXPECT_EQ("    -1.23", str(Format("{0:9}") << -1.23l));
  EXPECT_EQ("    0xcafe",
      str(Format("{0:10}") << reinterpret_cast<void*>(0xcafe)));
  EXPECT_EQ("x          ", str(Format("{0:11}") << 'x'));
  EXPECT_EQ("str         ", str(Format("{0:12}") << "str"));
  EXPECT_EQ("test         ", str(Format("{0:13}") << TestString("test")));
}

TEST(FormatterTest, Precision) {
  char format[BUFFER_SIZE];
  SPrintf(format, "{0:.%u", UINT_MAX);
  Increment(format + 4);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  std::size_t size = std::strlen(format);
  format[size] = '}';
  format[size + 1] = 0;
  EXPECT_THROW_MSG(Format(format) << 0,
      FormatError, "number is too big in format");

  SPrintf(format, "{0:.%u", INT_MAX + 1u);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  SPrintf(format, "{0:.%u}", INT_MAX + 1u);
  EXPECT_THROW_MSG(Format(format) << 0,
      FormatError, "number is too big in format");

  EXPECT_THROW_MSG(Format("{0:.") << 0,
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:.}") << 0,
      FormatError, "missing precision in format");
  EXPECT_THROW_MSG(Format("{0:.2") << 0,
      FormatError, "unmatched '{' in format");

  EXPECT_THROW_MSG(Format("{0:.2}") << 42,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << 42,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2}") << 42u,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << 42u,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2}") << 42l,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << 42l,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2}") << 42ul,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << 42ul,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_EQ("1.2", str(Format("{0:.2}") << 1.2345));
  EXPECT_EQ("1.2", str(Format("{0:.2}") << 1.2345l));

  EXPECT_THROW_MSG(Format("{0:.2}") << reinterpret_cast<void*>(0xcafe),
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << reinterpret_cast<void*>(0xcafe),
      FormatError, "precision specifier requires floating-point argument");

  EXPECT_THROW_MSG(Format("{0:.2}") << 'x',
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << 'x',
      FormatError, "precision specifier requires floating-point argument");

  EXPECT_THROW_MSG(Format("{0:.2}") << "str",
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << "str",
      FormatError, "precision specifier requires floating-point argument");

  EXPECT_THROW_MSG(Format("{0:.2}") << TestString(),
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.2f}") << TestString(),
      FormatError, "precision specifier requires floating-point argument");
}

TEST(FormatterTest, RuntimePrecision) {
  char format[BUFFER_SIZE];
  SPrintf(format, "{0:.{%u", UINT_MAX);
  Increment(format + 4);
  EXPECT_THROW_MSG(Format(format), FormatError, "unmatched '{' in format");
  std::size_t size = std::strlen(format);
  format[size] = '}';
  format[size + 1] = 0;
  EXPECT_THROW_MSG(Format(format) << 0, FormatError, "unmatched '{' in format");
  format[size + 1] = '}';
  format[size + 2] = 0;
  EXPECT_THROW_MSG(Format(format) << 0,
      FormatError, "number is too big in format");

  EXPECT_THROW_MSG(Format("{0:.{") << 0,
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:.{}") << 0,
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:.{x}}") << 0,
      FormatError, "invalid argument index in format string");
  EXPECT_THROW_MSG(Format("{0:.{1}") << 0 << 0,
      FormatError, "unmatched '{' in format");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0,
      FormatError, "argument index is out of range in format");

  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << -1,
      FormatError, "negative precision in format");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << (INT_MAX + 1u),
      FormatError, "number is too big in format");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << -1l,
      FormatError, "negative precision in format");
  if (sizeof(long) > sizeof(int)) {
    long value = INT_MAX;
    EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << (value + 1),
        FormatError, "number is too big in format");
  }
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << (INT_MAX + 1ul),
      FormatError, "number is too big in format");

  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << '0',
      FormatError, "precision is not integer");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 0 << 0.0,
      FormatError, "precision is not integer");

  EXPECT_THROW_MSG(Format("{0:.{1}}") << 42 << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << 42 << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 42u << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << 42u << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 42l << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << 42l << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}}") << 42ul << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << 42ul << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_EQ("1.2", str(Format("{0:.{1}}") << 1.2345 << 2));
  EXPECT_EQ("1.2", str(Format("{1:.{0}}") << 2 << 1.2345l));

  EXPECT_THROW_MSG(Format("{0:.{1}}") << reinterpret_cast<void*>(0xcafe) << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << reinterpret_cast<void*>(0xcafe) << 2,
      FormatError, "precision specifier requires floating-point argument");

  EXPECT_THROW_MSG(Format("{0:.{1}}") << 'x' << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << 'x' << 2,
      FormatError, "precision specifier requires floating-point argument");

  EXPECT_THROW_MSG(Format("{0:.{1}}") << "str" << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << "str" << 2,
      FormatError, "precision specifier requires floating-point argument");

  EXPECT_THROW_MSG(Format("{0:.{1}}") << TestString() << 2,
      FormatError, "precision specifier requires floating-point argument");
  EXPECT_THROW_MSG(Format("{0:.{1}f}") << TestString() << 2,
      FormatError, "precision specifier requires floating-point argument");
}

template <typename T>
void CheckUnknownTypes(
    const T &value, const char *types, const char *type_name) {
  char format[BUFFER_SIZE], message[BUFFER_SIZE];
  const char *special = ".0123456789}";
  for (int i = CHAR_MIN; i <= CHAR_MAX; ++i) {
    char c = i;
    if (std::strchr(types, c) || std::strchr(special, c) || !c) continue;
    SPrintf(format, "{0:10%c}", c);
    if (std::isprint(static_cast<unsigned char>(c)))
      SPrintf(message, "unknown format code '%c' for %s", c, type_name);
    else
      SPrintf(message, "unknown format code '\\x%02x' for %s", c, type_name);
    EXPECT_THROW_MSG(Format(format) << value, FormatError, message)
      << format << " " << message;
  }
}

TEST(FormatterTest, FormatInt) {
  EXPECT_THROW_MSG(Format("{0:v") << 42,
      FormatError, "unmatched '{' in format");
  CheckUnknownTypes(42, "doxX", "integer");
}

TEST(FormatterTest, FormatDec) {
  EXPECT_EQ("0", str(Format("{0}") << 0));
  EXPECT_EQ("42", str(Format("{0}") << 42));
  EXPECT_EQ("42", str(Format("{0:d}") << 42));
  EXPECT_EQ("42", str(Format("{0}") << 42u));
  EXPECT_EQ("-42", str(Format("{0}") << -42));
  EXPECT_EQ("12345", str(Format("{0}") << 12345));
  EXPECT_EQ("67890", str(Format("{0}") << 67890));
  char buffer[BUFFER_SIZE];
  SPrintf(buffer, "%d", INT_MIN);
  EXPECT_EQ(buffer, str(Format("{0}") << INT_MIN));
  SPrintf(buffer, "%d", INT_MAX);
  EXPECT_EQ(buffer, str(Format("{0}") << INT_MAX));
  SPrintf(buffer, "%u", UINT_MAX);
  EXPECT_EQ(buffer, str(Format("{0}") << UINT_MAX));
  SPrintf(buffer, "%ld", 0 - static_cast<unsigned long>(LONG_MIN));
  EXPECT_EQ(buffer, str(Format("{0}") << LONG_MIN));
  SPrintf(buffer, "%ld", LONG_MAX);
  EXPECT_EQ(buffer, str(Format("{0}") << LONG_MAX));
  SPrintf(buffer, "%lu", ULONG_MAX);
  EXPECT_EQ(buffer, str(Format("{0}") << ULONG_MAX));
}

TEST(FormatterTest, FormatHex) {
  EXPECT_EQ("0", str(Format("{0:x}") << 0));
  EXPECT_EQ("42", str(Format("{0:x}") << 0x42));
  EXPECT_EQ("42", str(Format("{0:x}") << 0x42u));
  EXPECT_EQ("-42", str(Format("{0:x}") << -0x42));
  EXPECT_EQ("12345678", str(Format("{0:x}") << 0x12345678));
  EXPECT_EQ("90abcdef", str(Format("{0:x}") << 0x90abcdef));
  EXPECT_EQ("12345678", str(Format("{0:X}") << 0x12345678));
  EXPECT_EQ("90ABCDEF", str(Format("{0:X}") << 0x90ABCDEF));
  char buffer[BUFFER_SIZE];
  SPrintf(buffer, "-%x", 0 - static_cast<unsigned>(INT_MIN));
  EXPECT_EQ(buffer, str(Format("{0:x}") << INT_MIN));
  SPrintf(buffer, "%x", INT_MAX);
  EXPECT_EQ(buffer, str(Format("{0:x}") << INT_MAX));
  SPrintf(buffer, "%x", UINT_MAX);
  EXPECT_EQ(buffer, str(Format("{0:x}") << UINT_MAX));
  SPrintf(buffer, "-%lx", 0 - static_cast<unsigned long>(LONG_MIN));
  EXPECT_EQ(buffer, str(Format("{0:x}") << LONG_MIN));
  SPrintf(buffer, "%lx", LONG_MAX);
  EXPECT_EQ(buffer, str(Format("{0:x}") << LONG_MAX));
  SPrintf(buffer, "%lx", ULONG_MAX);
  EXPECT_EQ(buffer, str(Format("{0:x}") << ULONG_MAX));
}

TEST(FormatterTest, FormatOct) {
  EXPECT_EQ("0", str(Format("{0:o}") << 0));
  EXPECT_EQ("42", str(Format("{0:o}") << 042));
  EXPECT_EQ("42", str(Format("{0:o}") << 042u));
  EXPECT_EQ("-42", str(Format("{0:o}") << -042));
  EXPECT_EQ("12345670", str(Format("{0:o}") << 012345670));
  char buffer[BUFFER_SIZE];
  SPrintf(buffer, "-%o", 0 - static_cast<unsigned>(INT_MIN));
  EXPECT_EQ(buffer, str(Format("{0:o}") << INT_MIN));
  SPrintf(buffer, "%o", INT_MAX);
  EXPECT_EQ(buffer, str(Format("{0:o}") << INT_MAX));
  SPrintf(buffer, "%o", UINT_MAX);
  EXPECT_EQ(buffer, str(Format("{0:o}") << UINT_MAX));
  SPrintf(buffer, "-%lo", 0 - static_cast<unsigned long>(LONG_MIN));
  EXPECT_EQ(buffer, str(Format("{0:o}") << LONG_MIN));
  SPrintf(buffer, "%lo", LONG_MAX);
  EXPECT_EQ(buffer, str(Format("{0:o}") << LONG_MAX));
  SPrintf(buffer, "%lo", ULONG_MAX);
  EXPECT_EQ(buffer, str(Format("{0:o}") << ULONG_MAX));
}

TEST(FormatterTest, FormatDouble) {
  CheckUnknownTypes(1.2, "eEfFgG", "double");
  EXPECT_EQ("0", str(Format("{0:}") << 0.0));
  EXPECT_EQ("0.000000", str(Format("{0:f}") << 0.0));
  EXPECT_EQ("392.65", str(Format("{0:}") << 392.65));
  EXPECT_EQ("392.65", str(Format("{0:g}") << 392.65));
  EXPECT_EQ("392.65", str(Format("{0:G}") << 392.65));
  EXPECT_EQ("392.650000", str(Format("{0:f}") << 392.65));
  EXPECT_EQ("392.650000", str(Format("{0:F}") << 392.65));
  char buffer[BUFFER_SIZE];
  SPrintf(buffer, "%e", 392.65);
  EXPECT_EQ(buffer, str(Format("{0:e}") << 392.65));
  SPrintf(buffer, "%E", 392.65);
  EXPECT_EQ(buffer, str(Format("{0:E}") << 392.65));
  EXPECT_EQ("+0000392.6", str(Format("{0:+010.4g}") << 392.65));
}

TEST(FormatterTest, FormatNaN) {
  double nan = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ("nan", str(Format("{}") << nan));
  EXPECT_EQ("+nan", str(Format("{:+}") << nan));
  EXPECT_EQ("-nan", str(Format("{}") << -nan));
  EXPECT_EQ(" nan", str(Format("{: }") << nan));
  EXPECT_EQ("NAN", str(Format("{:F}") << nan));
  EXPECT_EQ("nan    ", str(Format("{:<7}") << nan));
  EXPECT_EQ("  nan  ", str(Format("{:^7}") << nan));
  EXPECT_EQ("    nan", str(Format("{:>7}") << nan));
}

TEST(FormatterTest, FormatInfinity) {
  double inf = std::numeric_limits<double>::infinity();
  EXPECT_EQ("inf", str(Format("{}") << inf));
  EXPECT_EQ("+inf", str(Format("{:+}") << inf));
  EXPECT_EQ("-inf", str(Format("{}") << -inf));
  EXPECT_EQ(" inf", str(Format("{: }") << inf));
  EXPECT_EQ("INF", str(Format("{:F}") << inf));
  EXPECT_EQ("inf    ", str(Format("{:<7}") << inf));
  EXPECT_EQ("  inf  ", str(Format("{:^7}") << inf));
  EXPECT_EQ("    inf", str(Format("{:>7}") << inf));
}

TEST(FormatterTest, FormatLongDouble) {
  EXPECT_EQ("0", str(Format("{0:}") << 0.0l));
  EXPECT_EQ("0.000000", str(Format("{0:f}") << 0.0l));
  EXPECT_EQ("392.65", str(Format("{0:}") << 392.65l));
  EXPECT_EQ("392.65", str(Format("{0:g}") << 392.65l));
  EXPECT_EQ("392.65", str(Format("{0:G}") << 392.65l));
  EXPECT_EQ("392.650000", str(Format("{0:f}") << 392.65l));
  EXPECT_EQ("392.650000", str(Format("{0:F}") << 392.65l));
  char buffer[BUFFER_SIZE];
  SPrintf(buffer, "%Le", 392.65l);
  EXPECT_EQ(buffer, str(Format("{0:e}") << 392.65l));
  SPrintf(buffer, "%LE", 392.65l);
  EXPECT_EQ("+0000392.6", str(Format("{0:+010.4g}") << 392.65l));
}

TEST(FormatterTest, FormatChar) {
  CheckUnknownTypes('a', "c", "char");
  EXPECT_EQ("a", str(Format("{0}") << 'a'));
  EXPECT_EQ("z", str(Format("{0:c}") << 'z'));
}

TEST(FormatterTest, FormatCString) {
  CheckUnknownTypes("test", "s", "string");
  EXPECT_EQ("test", str(Format("{0}") << "test"));
  EXPECT_EQ("test", str(Format("{0:s}") << "test"));
  char nonconst[] = "nonconst";
  EXPECT_EQ("nonconst", str(Format("{0}") << nonconst));
  EXPECT_THROW_MSG(Format("{0}") << reinterpret_cast<const char*>(0),
      FormatError, "string pointer is null");
}

TEST(FormatterTest, FormatPointer) {
  CheckUnknownTypes(reinterpret_cast<void*>(0x1234), "p", "pointer");
  EXPECT_EQ("0x0", str(Format("{0}") << reinterpret_cast<void*>(0)));
  EXPECT_EQ("0x1234", str(Format("{0}") << reinterpret_cast<void*>(0x1234)));
  EXPECT_EQ("0x1234", str(Format("{0:p}") << reinterpret_cast<void*>(0x1234)));
  EXPECT_EQ("0x" + std::string(sizeof(void*) * CHAR_BIT / 4, 'f'),
      str(Format("{0}") << reinterpret_cast<void*>(~uintptr_t())));
}

TEST(FormatterTest, FormatString) {
  EXPECT_EQ("test", str(Format("{0}") << std::string("test")));
}

TEST(FormatterTest, FormatUsingIOStreams) {
  EXPECT_EQ("a string", str(Format("{0}") << TestString("a string")));
  std::string s = str(fmt::Format("The date is {0}") << Date(2012, 12, 9));
  EXPECT_EQ("The date is 2012-12-9", s);
  Date date(2012, 12, 9);
  CheckUnknownTypes(date, "", "object");
}

class Answer {};

template <typename Char>
void Format(BasicWriter<Char> &f, const fmt::FormatSpec &spec, Answer) {
  f.Write("42", spec);
}

TEST(FormatterTest, CustomFormat) {
  EXPECT_EQ("42", str(Format("{0}") << Answer()));
}

TEST(FormatterTest, FormatStringFromSpeedTest) {
  EXPECT_EQ("1.2340000000:0042:+3.13:str:0x3e8:X:%",
      str(Format("{0:0.10f}:{1:04}:{2:+g}:{3}:{4}:{5}:%")
          << 1.234 << 42 << 3.13 << "str"
          << reinterpret_cast<void*>(1000) << 'X'));
}

TEST(FormatterTest, FormatterCtor) {
  Formatter format;
  EXPECT_EQ(0u, format.size());
  EXPECT_STREQ("", format.c_str());
  EXPECT_EQ("", format.str());
  format("part{0}") << 1;
  format("part{0}") << 2;
  EXPECT_EQ("part1part2", format.str());
}

TEST(FormatterTest, FormatterAppend) {
  Formatter format;
  format("part{0}") << 1;
  EXPECT_EQ(strlen("part1"), format.size());
  EXPECT_STREQ("part1", format.c_str());
  EXPECT_STREQ("part1", format.data());
  EXPECT_EQ("part1", format.str());
  format("part{0}") << 2;
  EXPECT_EQ(strlen("part1part2"), format.size());
  EXPECT_STREQ("part1part2", format.c_str());
  EXPECT_STREQ("part1part2", format.data());
  EXPECT_EQ("part1part2", format.str());
}

TEST(FormatterTest, FormatterExamples) {
  using fmt::hex;
  EXPECT_EQ("0000cafe", str(BasicWriter<char>() << pad(hex(0xcafe), 8, '0')));

  std::string message = str(Format("The answer is {}") << 42);
  EXPECT_EQ("The answer is 42", message);

  EXPECT_EQ("42", str(Format("{}") << 42));
  EXPECT_EQ("42", str(Format(std::string("{}")) << 42));
  EXPECT_EQ("42", str(Format(Format("{{}}")) << 42));

  Formatter format;
  format("Current point:\n");
  format("({0:+f}, {1:+f})\n") << -3.14 << 3.14;
  EXPECT_EQ("Current point:\n(-3.140000, +3.140000)\n", format.str());

  {
    fmt::Formatter format;
    for (int i = 0; i < 10; i++)
      format("{0}") << i;
    std::string s = format.str(); // s == 0123456789
    EXPECT_EQ("0123456789", s);
  }
}

TEST(FormatterTest, ArgInserter) {
  Formatter format;
  EXPECT_EQ("1", str(format("{0}") << 1));
  EXPECT_STREQ("12", c_str(format("{0}") << 2));
}

TEST(FormatterTest, StrNamespace) {
  fmt::str(Format(""));
  fmt::c_str(Format(""));
}

TEST(FormatterTest, ExceptionInNestedFormat) {
  // Exception in nested format may cause Arg's destructor be called before
  // the argument has been attached to a Formatter object.
  EXPECT_THROW(Format(Format("{}")) << 42;, FormatError);
}

TEST(StringRefTest, Ctor) {
  EXPECT_STREQ("abc", StringRef("abc").c_str());
  EXPECT_EQ(3u, StringRef("abc").size());

  EXPECT_STREQ("defg", StringRef(std::string("defg")).c_str());
  EXPECT_EQ(4u, StringRef(std::string("defg")).size());
}

TEST(StringRefTest, ConvertToString) {
  std::string s = StringRef("abc");
  EXPECT_EQ("abc", s);
}

struct CountCalls {
  int &num_calls;

  CountCalls(int &num_calls) : num_calls(num_calls) {}

  void operator()(const Formatter &) const {
    ++num_calls;
  }
};

TEST(TempFormatterTest, Action) {
  int num_calls = 0;
  {
    fmt::TempFormatter<char, CountCalls> af("test", CountCalls(num_calls));
    EXPECT_EQ(0, num_calls);
  }
  EXPECT_EQ(1, num_calls);
}

TEST(TempFormatterTest, ActionNotCalledOnError) {
  int num_calls = 0;
  {
    typedef fmt::TempFormatter<char, CountCalls> TestFormatter;
    EXPECT_THROW(TestFormatter af("{0", CountCalls(num_calls)), FormatError);
  }
  EXPECT_EQ(0, num_calls);
}

// The test doesn't compile on older compilers which follow C++03 and
// require an accessible copy constructor when binding a temporary to
// a const reference.
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 7
TEST(TempFormatterTest, ArgLifetime) {
  // The following code is for testing purposes only. It is a definite abuse
  // of the API and shouldn't be used in real applications.
  const fmt::TempFormatter<char> &af = fmt::Format("{0}");
  const_cast<fmt::TempFormatter<char>&>(af) << std::string("test");
  // String object passed as an argument to TempFormatter has
  // been destroyed, but ArgInserter dtor hasn't been called yet.
  // But that's OK since the Arg's dtor takes care of this and
  // calls Format.
}
#endif

TEST(TempFormatterTest, ConvertToStringRef) {
  EXPECT_STREQ("abc", StringRef(Format("a{0}c") << 'b').c_str());
  EXPECT_EQ(3u, StringRef(Format("a{0}c") << 'b').size());
}

struct PrintError {
  void operator()(const fmt::Formatter &f) const {
    std::cerr << "Error: " << f.str() << std::endl;
  }
};

fmt::TempFormatter<char, PrintError> ReportError(const char *format) {
  return fmt::TempFormatter<char, PrintError>(format);
}

TEST(TempFormatterTest, Examples) {
  EXPECT_EQ("First, thou shalt count to three",
      str(Format("First, thou shalt count to {0}") << "three"));
  EXPECT_EQ("Bring me a shrubbery",
      str(Format("Bring me a {}") << "shrubbery"));
  EXPECT_EQ("From 1 to 3", str(Format("From {} to {}") << 1 << 3));

  char buffer[BUFFER_SIZE];
  SPrintf(buffer, "%03.2f", -1.2);
  EXPECT_EQ(buffer, str(Format("{:03.2f}") << -1.2));

  EXPECT_EQ("a, b, c", str(Format("{0}, {1}, {2}") << 'a' << 'b' << 'c'));
  EXPECT_EQ("a, b, c", str(Format("{}, {}, {}") << 'a' << 'b' << 'c'));
  EXPECT_EQ("c, b, a", str(Format("{2}, {1}, {0}") << 'a' << 'b' << 'c'));
  EXPECT_EQ("abracadabra", str(Format("{0}{1}{0}") << "abra" << "cad"));

  EXPECT_EQ("left aligned                  ",
      str(Format("{:<30}") << "left aligned"));
  EXPECT_EQ("                 right aligned",
      str(Format("{:>30}") << "right aligned"));
  EXPECT_EQ("           centered           ",
      str(Format("{:^30}") << "centered"));
  EXPECT_EQ("***********centered***********",
      str(Format("{:*^30}") << "centered"));

  EXPECT_EQ("+3.140000; -3.140000",
      str(Format("{:+f}; {:+f}") << 3.14 << -3.14));
  EXPECT_EQ(" 3.140000; -3.140000",
      str(Format("{: f}; {: f}") << 3.14 << -3.14));
  EXPECT_EQ("3.140000; -3.140000",
      str(Format("{:-f}; {:-f}") << 3.14 << -3.14));

  EXPECT_EQ("int: 42;  hex: 2a;  oct: 52",
      str(Format("int: {0:d};  hex: {0:x};  oct: {0:o}") << 42));
  EXPECT_EQ("int: 42;  hex: 0x2a;  oct: 052",
      str(Format("int: {0:d};  hex: {0:#x};  oct: {0:#o}") << 42));

  std::string path = "somefile";
  ReportError("File not found: {0}") << path;
}

template <typename T>
std::string str(const T &value) {
  return fmt::str(fmt::Format("{0}") << value);
}

TEST(StrTest, Convert) {
  EXPECT_EQ("42", str(42));
  std::string s = str(Date(2012, 12, 9));
  EXPECT_EQ("2012-12-9", s);
}

int main(int argc, char **argv) {
#ifdef _WIN32
  // Disable message boxes on assertion failures.
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
