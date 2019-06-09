#include <c4/conf/conf.hpp>
#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>
#include <c4/std/string.hpp>


using MultipleContents = std::initializer_list<c4::csubstr>;
struct MultipleFiles
{
    std::vector<c4::fs::ScopedTmpFile> m_files;
    std::vector<c4::csubstr> m_filenames;

    MultipleFiles(MultipleContents contents)
        : m_files(contents.begin(), contents.end()),
          m_filenames()
    {
        m_filenames.reserve(m_files.size());
        for(auto const& f : m_files)
        {
            m_filenames.emplace_back(c4::to_csubstr(f.name()));
        }
    }
};


void test_same(MultipleContents contents, c4::csubstr expected_in)
{
    MultipleFiles mf(contents);
    ASSERT_EQ(mf.m_files.size(), contents.size());
    ASSERT_EQ(mf.m_filenames.size(), contents.size());

    c4::yml::Tree tree_result, tree_expected;
    bool got_one = c4::conf::load(mf.m_filenames, &tree_result);
    c4::yml::parse(expected_in, &tree_expected);

    auto result = c4::yml::emitrs<std::string>(tree_result);
    auto expected = c4::yml::emitrs<std::string>(tree_expected);

    EXPECT_EQ(result, expected);
}



TEST(basic, orthogonal)
{
    test_same(
        {"a: 0", "b: 1", "c: 2"},
        "{a: 0, b: 1, c: 2}"
    );
}

TEST(basic, overriding)
{
    test_same(
        {"a: 0", "{a: 1, b: 1}", "c: 2"},
        "{a: 1, b: 1, c: 2}"
    );
}
