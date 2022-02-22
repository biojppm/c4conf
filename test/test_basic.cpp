#include <c4/std/string.hpp>
#include <c4/conf/conf.hpp>
#include <c4/fs/fs.hpp>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <initializer_list>
#include <vector>


using MultipleConfsSpec = std::initializer_list<c4::csubstr>;
using MultipleFilesSpec = std::initializer_list<c4::csubstr>;


// from multiple strings, create corresponding multiple files
// to allow loading from the file system
struct MultipleFiles
{
    std::vector<c4::fs::ScopedTmpFile> m_files;
    MultipleFiles(MultipleFilesSpec contents)
        : m_files(contents.begin(), contents.end())
    {
        size_t i = 0;
        for(c4::csubstr cont : contents)
        {
            INFO("i=", i);
            std::string actual = c4::fs::file_get_contents<std::string>(m_files[i].name());
            CHECK_EQ(c4::to_csubstr(actual), cont);
            i++;
        }
    }
};

std::string emitstr(c4::yml::Tree const& tree)
{
    return c4::yml::emitrs<std::string>(tree);
}

// apply multiple files, then apply multiple confs
void test_same(MultipleFilesSpec files, MultipleConfsSpec confs, c4::csubstr expected_yml)
{
    MultipleFiles mf(files);
    REQUIRE_EQ(mf.m_files.size(), files.size());

    c4::yml::Tree tree_result, tree_expected;

    c4::conf::Workspace ws(&tree_result);
    for(const auto &file : mf.m_files)
        ws.prepare_add_file(file.name());
    for(c4::csubstr spec : confs)
        ws.prepare_add_conf(spec);
    CHECK_GT(tree_result.arena_capacity(), 0u);
    CHECK_EQ(tree_result.arena_size(), 0u);

    for(const auto &file : mf.m_files)
        ws.add_file(file.name());
    for(c4::csubstr spec : confs)
        ws.add_conf(spec);
    c4::yml::parse_in_arena(expected_yml, &tree_expected);

    std::string result = emitstr(tree_result);
    std::string expected = emitstr(tree_expected);

    CHECK_EQ(expected, result);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("basic.orthogonal_settings_do_not_interfere")
{
    test_same(
        {"a: 0", "b: 1", "c: 2"},
        {},
        "{a: 0, b: 1, c: 2}"
    );
}

TEST_CASE("basic.overriding_from_two_files")
{
    test_same(
        {"a: 0", "a: 1"},
        {},
        "a: 1"
    );
}

TEST_CASE("basic.overriding_from_three_files")
{
    test_same(
        {"a: 0", "a: 1", "a: 2"},
        {},
        "a: 2"
    );
}

TEST_CASE("basic.overriding_from_four_files")
{
    test_same(
        {"a: 0", "a: 1", "a: 2", "a: 3"},
        {},
        "a: 3"
    );
}

TEST_CASE("basic.orthogonal_and_overriding_from_four_files")
{
    test_same(
        {"a: 0", "b: 1", "a: 2", "b: 3"},
        {},
        "{a: 2, b: 3}"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("overriding_val.with_val")
{
    test_same(
        {"a: 0", "a: 1", "a: 2", "a: foo"},
        {},
        "a: foo"
    );
}

TEST_CASE("overriding_val.with_map")
{
    test_same(
        {"a: 0", "a: 1", "a: 2", "a: {foo: bar}"},
        {},
        "a: {foo: bar}"
    );
}

TEST_CASE("overriding_val.with_seq")
{
    test_same(
        {"a: 0", "a: 1", "a: 2", "a: [foo, bar]"},
        {},
        "a: [foo, bar]"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("overriding_map.with_val")
{
    test_same(
        {"a: 0", "a: 1", "a: {foo: bar}", "a: not"},
        {},
        "a: not"
    );
}

TEST_CASE("overriding_map.with_map_merging")
{
    test_same(
        {"a: 0", "a: 1", "a: {foo: bar}", "a: {bar: foo}"},
        {},
        "a: {foo: bar, bar: foo}"
    );
}

TEST_CASE("overriding_map.with_map_replacing")
{
    test_same(
        {"a: 0", "a: 1", "a: {foo: bar}", "a: ~", "a: {bar: foo}"},
        {},
        "a: {bar: foo}"
    );
}

TEST_CASE("overriding_map.with_seq")
{
    test_same(
        {"a: 0", "a: 1", "a: {foo: bar}", "a: [foo, bar]"},
        {},
        "a: [foo, bar]"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("overriding_seq.with_val")
{
    test_same(
        {"a: 0", "a: 1", "a: [foo, bar]", "a: not"},
        {},
        "a: not"
    );
}

TEST_CASE("overriding_seq.with_map")
{
    test_same(
        {"a: 0", "a: 1", "a: [foo, bar]", "a: {bar: foo}"},
        {},
        "a: {bar: foo}"
    );
}

TEST_CASE("overriding_seq.with_seq_merging")
{
    test_same(
        {"a: 0", "a: 1", "a: [foo, bar]", "a: [bar, foo]"},
        {},
        "a: [foo, bar, bar, foo]"
    );
}

TEST_CASE("overriding_seq.with_seq_replacing")
{
    test_same(
        {"a: 0", "a: 1", "a: [foo, bar]", "a: ~", "a: [bar, foo]"},
        {},
        "a: [bar, foo]"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("files_and_confs.baseline")
{
    test_same(
        {"a: 0", "{a: 1, b: 1}", "c: 2"},
        {},
        "{a: 1, b: 1, c: 2}"
    );
}

TEST_CASE("files_and_confs.non_nested_vals")
{
    test_same(
        {"a: 0", "{a: 1, b: 1}", "c: 2"},
        {"a=3", "a=4", "d=5", "d=6"},
        "{a: 4, b: 1, c: 2, d: 6}"
    );
}

TEST_CASE("files_and_confs.map_and_seq")
{
    auto t = c4::yml::parse_in_arena("[0, 1, 2]");
    REQUIRE_EQ(t.size(), 4u);
    REQUIRE_EQ(t.num_children(0u), 3u);
    test_same(
        {"a: 0", "{a: 1, b: 1}", "c: 2"},
        {"a=3", "a=4", "d=5", "d={e: 5}", "f=[0, 1, 2]"},
        "{a: 4, b: 1, c: 2, d: {e: 5}, f: [0, 1, 2]}"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("nested_map_lookup.override_with_val")
{
    test_same(
        {"d: {e: 4, f: {g: 5}}"},
        {"d.e=6", "d.f.g=7"},
         "d: {e: 6, f: {g: 7}}"
    );
}

TEST_CASE("nested_map_lookup.override_with_map")
{
    test_same(
        {"d: {e: 4, f: {g: 5}}"},
        {"d.e=6", "d.f.g={now: this is a map}"},
         "d: {e: 6, f: {g: {now: this is a map}}}"
    );
}

TEST_CASE("nested_map_lookup.override_with_map_twice")
{
    test_same(
        {"d: {e: 4, f: {g: 5}}"},
        {"d.e=6",
         "d.f.g={now: this is a map}",
         "d.f.g={and: {this: is another map}}"},
         "d: {e: 6, f: {g: {now: this is a map, and: {this: is another map}}}}"
    );
}

TEST_CASE("nested_map_lookup.override_with_map_twice_then_val")
{
    test_same(
        {"d: {e: 4, f: {g: 5}}"},
        {"d.e=6",
         "d.f.g={now: this is a map}",
         "d.f.g={and: {this: is another map}}",
         "d.f.g='easy now, this is a scalar'",
        },
         "d: {e: 6, f: {g: 'easy now, this is a scalar'}}"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("nested_seq_lookup.override_val_with_val")
{
    test_same(
        {"{seqs: [[[000, 001, 002], [010, 011, 012]], [[100, 101, 102], [110, 111, 112]], [[200, 201, 202], [210, 211, 212]]]}"},
        {"seqs[0][0][0]=```"},
         "{seqs: [[[```, 001, 002], [010, 011, 012]], [[100, 101, 102], [110, 111, 112]], [[200, 201, 202], [210, 211, 212]]]}"
    );
}

TEST_CASE("nested_seq_lookup.override_seq_with_val")
{
    test_same(
        {"{seqs: [[[000, 001, 002], [010, 011, 012]], [[100, 101, 102], [110, 111, 112]], [[200, 201, 202], [210, 211, 212]]]}"},
        {"seqs[0][0][0]=```",
         "seqs[0][1]=aaa",
         "seqs[1]=bbb",
         "seqs[2]=ccc",
        },
        "{seqs: [[[```, 001, 002], aaa], bbb, ccc]}"
    );
}

TEST_CASE("nested_seq_lookup.override_all_with_val")
{
    test_same(
        {"{seqs: [[[000, 001, 002], [010, 011, 012]], [[100, 101, 102], [110, 111, 112]], [[200, 201, 202], [210, 211, 212]]]}"},
        {"seqs[0][0][0]=```",
         "seqs[0][1]=aaa",
         "seqs[1]=bbb",
         "seqs=fdx!",
        },
        "{seqs: fdx!}"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("nested_mixed_lookup.override_val_with_val")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[0]=```",
         "map.seq[1].map.seq[1]=@@@",
         "map.seq[1].and=hell yeah",
        },
        "{map: {seq: [```, {map: {seq: [10, @@@]}, and: hell yeah}]}}"
    );
}

TEST_CASE("nested_mixed_lookup.override_val_with_seq")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[0]=[foo, bar]",
         "map.seq[1].map.seq[1]=[foo.bar]",
         "map.seq[1].and=[40, 60]",
        },
        "{map: {seq: [[foo, bar], {map: {seq: [10, [foo.bar]]}, and: [40, 60]}]}}"
    );
}

TEST_CASE("nested_mixed_lookup.override_val_with_map")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[0]={foo: bar}",
         "map.seq[1].map.seq[1]={foo.bar}",
         "map.seq[1].and={40: 60}",
        },
        "{map: {seq: [{foo: bar}, {map: {seq: [10, {foo.bar: }]}, and: {40: 60}}]}}"
    );
}

//-----------------------------------------------------------------------------

TEST_CASE("nested_mixed_lookup.override_seq_with_val")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=@@@"},
        "{map: {seq: [0, {map: {seq: @@@}, and: val}]}}"
    );
}

TEST_CASE("nested_mixed_lookup.override_seq_with_val_twice")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=@@@", "map.seq=```"},
        "{map: {seq: ```}}"
    );
}

TEST_CASE("nested_mixed_lookup.override_seq_with_seq")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=[a, b]"},
        "{map: {seq: [0, {map: {seq: [10, 20, a, b]}, and: val}]}}"
    );
}

TEST_CASE("nested_mixed_lookup.override_seq_with_seq_twice")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=~",
         "map.seq[1].map.seq=[a, b]"},
        "{map: {seq: [0, {map: {seq: [a, b]}, and: val}]}}"
    );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST_CASE("nested_mixed_lookup.create_seqs")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=~",
         "map.seq[1].map.seq=[10, 20]",
         "map.seq[1].map.seq=[[30, 40]]",
         "map.seq[1].map.seq=[[[50, 60], [70, 80]]]",
        },
        "{map: {seq: [0, {map: {seq: [10, 20, [30, 40], [[50, 60], [70, 80]]]}, and: val}]}}"
    );
}

TEST_CASE("nested_mixed_lookup.create_seq_entries")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=~",
         "map.seq[1].map.seq=[00, 10]",
         "map.seq[1].map.seq[4]=[40, 50]",
        },
        "{map: {seq: [0, {map: {seq: [00, 10, , , [40, 50]]}, and: val}]}}"
    );
}

TEST_CASE("nested_mixed_lookup.create_maps")
{
    test_same(
        {"{map: {seq: [0, {map: {seq: [10, 20]}, and: val}]}}"},
        {"map.seq[1].map.seq=~",
         "map.seq[1].map.seq={foo: bar}",
         "map.seq[1].map.seq={foo: bar, bar: {baz: bat}}",
        },
        "{map: {seq: [0, {map: {seq: {foo: bar, bar: {baz: bat}}}, and: val}]}}"
    );
}
