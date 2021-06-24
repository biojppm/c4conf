#include <c4/std/string.hpp>
#include <c4/conf/conf.hpp>
#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>


struct Conf
{
    c4::csubstr key, val;
};
using MultipleConfsSpec = std::initializer_list<Conf>;
using MultipleFilesSpec = std::initializer_list<c4::csubstr>;

struct MultipleFiles
{
    std::vector<c4::fs::ScopedTmpFile> m_files;
    std::vector<c4::csubstr> m_filenames;

    MultipleFiles(MultipleFilesSpec contents)
        : m_files(contents.begin(), contents.end()),
          m_filenames()
    {
        m_filenames.reserve(m_files.size());
        for(auto const& f : m_files)
            m_filenames.emplace_back(c4::to_csubstr(f.name()));
    }
};


void test_same(MultipleFilesSpec files, MultipleConfsSpec confs, c4::csubstr expected_in)
{
    MultipleFiles mf(files);
    ASSERT_EQ(mf.m_files.size(), files.size());
    ASSERT_EQ(mf.m_filenames.size(), files.size());

    c4::yml::Tree tree_result, tree_expected;
    bool got_one = c4::conf::add_files(mf.m_filenames, &tree_result);
    EXPECT_TRUE(got_one);
    c4::yml::parse(expected_in, &tree_expected);
    c4::conf::Workspace ws;
    for(auto const& conf : confs)
        c4::conf::add_conf(conf.key, conf.val, &tree_result, &ws);

    auto result = c4::yml::emitrs<std::string>(tree_result);
    auto expected = c4::yml::emitrs<std::string>(tree_expected);

    EXPECT_EQ(result, expected);
}



TEST(basic, orthogonal)
{
    test_same(
        {"a: 0", "b: 1", "c: 2"},
        {},
        "{a: 0, b: 1, c: 2}"
    );
}

TEST(basic, overriding0)
{
    test_same(
        {"a: 0", "a: 1"},
        {},
        "a: 1"
    );
}

TEST(basic, overriding1)
{
    test_same(
        {"a: 0", "a: 1", "a: 2"},
        {},
        "a: 2"
    );
}

TEST(basic, overriding2)
{
    test_same(
        {"a: 0", "b: 1", "a: 2"},
        {},
        "{a: 2, b: 1}"
    );
}

TEST(basic, overriding3)
{
    test_same(
        {"a: 0", "{a: 1, b: 1}", "c: 2"},
        {},
        "{a: 1, b: 1, c: 2}"
    );
}

TEST(basic, overriding_with_conf)
{
    test_same(
        {"a: 0", "{a: 1, b: 1}", "c: 2"},
        {{"a", "3"}, {"a", "4"}, {"d", "5"}},
        "{a: 4, b: 1, c: 2, d: 5}"
    );
}
