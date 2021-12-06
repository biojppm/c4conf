#include <c4/std/string.hpp>
#include <c4/conf/conf.hpp>
#include <c4/fs/fs.hpp>
#include <c4/span.hpp>
#include <gtest/gtest.h>

#include <vector>
#include <string>

namespace c4 {
namespace conf {

void test_opts(std::vector<std::string> const& input_args,
               std::vector<std::string> const& filtered_args,
               cspan<OptArg> expected_args,
               yml::Tree const& expected_tree);

void to_args(std::vector<std::string> const& stringvec, std::vector<char*> *args);


const csubstr reftree = R"(
key0:
  key0val0:
    - key0val0val0
    - key0val0val1
    - key0val0val2
  key0val1:
    - key0val1val0
    - key0val1val1
    - key0val1val2
key1:
  key1val0:
    - key1val0val0
    - key1val0val1
    - key1val0val2
  key1val1:
    - key1val1val0
    - key1val1val1
    - key1val1val2
)";

const OptSpec specs_buf[] = {
    {"-n", "--node", "set node value", Opt::set_node},
    {"-f", "--file", "load file"     , Opt::load_file},
    {"-d", "--dir" , "load dir"      , Opt::load_dir},
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST(opts, args_are_not_changed_when_given_insufficient_output_buffer)
{
    std::vector<std::string> originalbuf = {"-n", "key0=val0", "-b", "b0", "-n", "key1=val1", "-c", "c0", "c1"};
    std::vector<std::string> expectedbuf = {                   "-b", "b0",                    "-c", "c0", "c1"};
    std::vector<char *> original;
    std::vector<char *> expected;
    to_args(originalbuf, &original);
    to_args(expectedbuf, &expected);
    ASSERT_GT(originalbuf.size(), expectedbuf.size());
    size_t num_opts = 2u;
    std::vector<OptArg> output;
    std::vector<char *> actual = original;
    int argc = (int)actual.size();
    char ** argv = actual.data();
    size_t ret;
    ret = parse_opts(&argc, &argv,
                     specs_buf, C4_COUNTOF(specs_buf),
                     output.data(), output.size());
    ASSERT_NE(ret, argerror);
    EXPECT_EQ(ret, num_opts);
    EXPECT_EQ((size_t)argc, actual.size());
    EXPECT_EQ(argv, actual.data());
    EXPECT_EQ(actual, original);
    output.resize(ret);
    ret = parse_opts(&argc, &argv,
                     specs_buf, C4_COUNTOF(specs_buf),
                     output.data(), output.size());
    EXPECT_EQ(ret, num_opts);
    EXPECT_EQ((size_t)argc, expected.size());
    actual.resize((size_t)argc);
    EXPECT_EQ(*argv, original[2]); // the first non-filtered option
    for(size_t i = 0; i < (size_t)argc; ++i)
    {
        EXPECT_STREQ(actual[i], expected[i]) << "i=" << i;
    }
}

TEST(opts, empty_is_ok)
{
    cspan<OptArg> expected_args = {};
    yml::Tree expected_tree = yml::parse(reftree);
    test_opts({"-a", "-b", "b0", "-c", "c0", "c1"},
              {"-a", "-b", "b0", "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST(opts, set_node)
{
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::set_node, "key1.key1val0[1]", "here it is"},
        {Opt::set_node, "key1.key1val1", "now this is a scalar"},
        {Opt::set_node, "key1.key1val0[1]", "here it is overrided"},
    };
    expected_tree["key1"]["key1val0"][1].set_val("here it is");
    expected_tree["key1"]["key1val1"].clear_children();
    expected_tree["key1"]["key1val1"].set_type(yml::KEYVAL);
    expected_tree["key1"]["key1val1"].set_val("now this is a scalar");
    expected_tree["key1"]["key1val0"][1].set_val("here it is overrided");
    // quotes in the value
    test_opts({"-a", "-n", "key1.key1val0[1]='here it is'", "-b", "b0", "--node", "key1.key1val1=\"now this is a scalar\"", "-c", "c0", "c1", "-n", "key1.key1val0[1]='here it is overrided'"},
              {"-a",                                        "-b", "b0",                                                     "-c", "c0", "c1"},
              expected_args,
              expected_tree);
    // quotes in the arg
    test_opts({"-a", "-n", "'key1.key1val0[1]=here it is'", "-b", "b0", "--node", "\"key1.key1val1=now this is a scalar\"", "-c", "c0", "c1", "-n", "'key1.key1val0[1]=here it is overrided'"},
              {"-a",                                        "-b", "b0",                                                     "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST(opts, set_node_with_nonscalars)
{
    csubstr k1k1v01_ = "{nothing: really, actually: something}";
    csubstr k1k1v1 = "[more, items, like, this, are, appended]";
    csubstr k1k1v01 = "{Jacquesson: [741, 742], Gosset: Grande Reserve}";
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::set_node, "key1.key1val0[1]", k1k1v01_},
        {Opt::set_node, "key1.key1val1", k1k1v1},
        {Opt::set_node, "key1.key1val0[1]", k1k1v01},
    };
    expected_tree["key1"]["key1val0"][1].change_type(yml::MAP);
    expected_tree["key1"]["key1val0"][1]["nothing"] = "really";
    expected_tree["key1"]["key1val0"][1]["actually"] = "something";
    expected_tree["key1"]["key1val1"][3] = "more";
    expected_tree["key1"]["key1val1"][4] = "items";
    expected_tree["key1"]["key1val1"][5] = "like";
    expected_tree["key1"]["key1val1"][6] = "this";
    expected_tree["key1"]["key1val1"][7] = "are";
    expected_tree["key1"]["key1val1"][8] = "appended";
    expected_tree["key1"]["key1val0"][1]["Jacquesson"] |= yml::SEQ;
    expected_tree["key1"]["key1val0"][1]["Jacquesson"][0] = "741";
    expected_tree["key1"]["key1val0"][1]["Jacquesson"][1] = "742";
    expected_tree["key1"]["key1val0"][1]["Gosset"] = "Grande Reserve";
    // quotes in the value
    test_opts({"-a", "-n", "key1.key1val0[1]='{nothing: really, actually: something}'", "-b", "b0", "--node", "key1.key1val1=\"[more, items, like, this, are, appended]\"", "-c", "c0", "c1", "-n", "key1.key1val0[1]='{Jacquesson: [741, 742], Gosset: Grande Reserve}'"},
              {"-a",                                                                    "-b", "b0",                                                                         "-c", "c0", "c1"},
              expected_args,
              expected_tree);
    // quotes in the arg
    test_opts({"-a", "-n", "'key1.key1val0[1]={nothing: really, actually: something}'", "-b", "b0", "--node", "\"key1.key1val1=[more, items, like, this, are, appended]\"", "-c", "c0", "c1", "-n", "'key1.key1val0[1]={Jacquesson: [741, 742], Gosset: Grande Reserve}'"},
              {"-a",                                                                    "-b", "b0",                                                                         "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

struct case1files
{
    case1files()
    {
        fs::mkdir("somedir");
        fs::file_put_contents("somedir/file0", csubstr("{key0: {key0val0: now replaced as a scalar}}"));
        fs::file_put_contents("somedir/file2", csubstr("{key0: {key0val0: NOW replaced as a scalar v2}}"));
        fs::file_put_contents("somedir/file3", csubstr("{key1: {key1val0: THIS one too v2}}"));
        fs::file_put_contents("somedir/file1", csubstr("{key1: {key1val0: this one too}}"));
        // these are all equivalent:
        fs::mkdir("somedir_to_node");
        fs::mkdir("somedir_to_key0");
        fs::mkdir("somedir_to_key1");
        fs::file_put_contents("somedir_to_node/file0", csubstr("{key0val0: now replaced as a scalar}"));
        fs::file_put_contents("somedir_to_node/file2", csubstr("{key0val0: NOW replaced as a scalar v2}"));
        fs::file_put_contents("somedir_to_key0/file0", csubstr("{key0val0: now replaced as a scalar}"));
        fs::file_put_contents("somedir_to_key0/file2", csubstr("{key0val0: NOW replaced as a scalar v2}"));
        fs::file_put_contents("somedir_to_node/file3", csubstr("{key1val0: THIS one too v2}"));
        fs::file_put_contents("somedir_to_node/file1", csubstr("{key1val0: this one too}"));
        fs::file_put_contents("somedir_to_key1/file3", csubstr("{key1val0: THIS one too v2}"));
        fs::file_put_contents("somedir_to_key1/file1", csubstr("{key1val0: this one too}"));
    }
    ~case1files()
    {
        fs::rmtree("somedir");
        fs::rmtree("somedir_to_node");
        fs::rmtree("somedir_to_key0");
        fs::rmtree("somedir_to_key1");
    }
    void transform1(yml::Tree *tree)
    {
        (*tree)["key0"]["key0val0"].clear_children();
        (*tree)["key0"]["key0val0"].set_type(yml::KEYVAL);
        (*tree)["key0"]["key0val0"].set_val("now replaced as a scalar");
        (*tree)["key1"]["key1val0"].clear_children();
        (*tree)["key1"]["key1val0"].set_type(yml::KEYVAL);
        (*tree)["key1"]["key1val0"].set_val("this one too");
    }
    void transform2(yml::Tree *tree)
    {
        (*tree)["key0"]["key0val0"].clear_children();
        (*tree)["key0"]["key0val0"].set_type(yml::KEYVAL);
        (*tree)["key0"]["key0val0"].set_val("NOW replaced as a scalar v2");
        (*tree)["key1"]["key1val0"].clear_children();
        (*tree)["key1"]["key1val0"].set_type(yml::KEYVAL);
        (*tree)["key1"]["key1val0"].set_val("THIS one too v2");
    }
};

TEST(opts, load_file)
{
    case1files setup;
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::load_file, {}, "somedir/file0"},
        {Opt::load_file, {}, "somedir/file1"},
    };
    setup.transform1(&expected_tree);
    test_opts({"-a", "-f", "somedir/file0", "-b", "b0", "--file", "somedir/file1", "-c", "c0", "c1"},
              {"-a",                        "-b", "b0",                            "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST(opts, load_file_to_node)
{
    case1files setup;
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::load_file, "key0", "somedir_to_node/file0"},
        {Opt::load_file, "key1", "somedir_to_node/file1"},
    };
    setup.transform1(&expected_tree);
    test_opts({"-a", "-f", "key0=somedir_to_node/file0", "-b", "b0", "--file", "key1=somedir_to_node/file1", "-c", "c0", "c1"},
              {"-a",                                     "-b", "b0",                                         "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST(opts, load_dir)
{
    case1files setup;
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::load_dir, {}, "somedir"},
    };
    setup.transform2(&expected_tree);
    test_opts({"-a", "-d", "somedir", "-b", "b0", "-c", "c0", "c1"},
              {"-a",                  "-b", "b0", "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST(opts, load_dir_to_node)
{
    case1files setup;
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::load_dir, "key0", "somedir_to_key0"},
        {Opt::load_dir, "key1", "somedir_to_key1"},
    };
    setup.transform2(&expected_tree);
    test_opts({"-a", "-d", "key0=somedir_to_key0", "-b", "b0", "-d", "key1=somedir_to_key1", "-c", "c0", "c1"},
              {"-a",                               "-b", "b0",                               "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void to_args(std::vector<std::string> const& stringvec, std::vector<char*> *args)
{
    args->resize(stringvec.size());
    for(size_t i = 0; i < stringvec.size(); ++i)
        (*args)[i] = (char *)&(stringvec[i][0]);
}

size_t parse_opts(int *argc, char ***argv, c4::span<OptArg> *opt_args)
{
    cspan<OptSpec> specs = specs_buf;
    size_t required = parse_opts(argc, argv,
                                 specs.data(), specs.size(),
                                 opt_args ? opt_args->data() : nullptr, opt_args ? opt_args->size() : 0);
    if(required == argerror)
        return argerror;
    if(opt_args && required <= opt_args->size())
        *opt_args = opt_args->first(required);
    return required;
}

void test_opts(std::vector<std::string> const& input_args,
               std::vector<std::string> const& filtered_args,
               cspan<OptArg> expected_args,
               yml::Tree const& expected_tree)
{
    std::vector<char*> input_args_ptr, filtered_args_ptr, input_args_ptr_orig;
    int argc;
    char **argv;
    auto reset_args = [&]{
        to_args(input_args, &input_args_ptr);
        to_args(filtered_args, &filtered_args_ptr);
        argc = (int) input_args_ptr.size();
        argv = input_args_ptr.data();
        input_args_ptr_orig = input_args_ptr;
    };
    auto check_input_args = [&]{
        EXPECT_EQ(argc, (int)input_args.size());
        for(int iarg = 0; iarg < argc; ++iarg)
            EXPECT_STREQ(input_args_ptr[(size_t)iarg], input_args_ptr_orig[(size_t)iarg]) << "i=" << iarg;
    };
    // must accept nullptr
    reset_args();
    size_t ret = parse_opts(&argc, &argv, nullptr);
    ASSERT_NE(ret, argerror);
    EXPECT_EQ(ret, expected_args.size());
    check_input_args();
    // must deal with insufficient buffer size
    reset_args();
    std::vector<OptArg> buf;
    buf.resize(expected_args.size() / 2);
    span<OptArg> buf_out = {buf.data(), buf.size()};
    ret = parse_opts(&argc, &argv, &buf_out);
    ASSERT_NE(ret, argerror);
    EXPECT_EQ(ret, expected_args.size());
    EXPECT_EQ(argc, (int)input_args.size());
    EXPECT_EQ(buf_out.size(), buf.size());
    EXPECT_EQ(buf_out.data(), buf.data());
    check_input_args();
    // must deal with sufficient buffer size
    reset_args();
    buf.resize(expected_args.size());
    buf_out = {buf.data(), buf.size()};
    ret = parse_opts(&argc, &argv, &buf_out);
    ASSERT_NE(ret, argerror);
    EXPECT_EQ(ret, expected_args.size());
    EXPECT_EQ(argc, (int)filtered_args.size());
    EXPECT_EQ(buf_out.size(), expected_args.size());
    EXPECT_EQ(buf_out.data(), buf.data());
    for(int iarg = 0; iarg < argc; ++iarg)
        EXPECT_EQ(to_csubstr(input_args_ptr[(size_t)iarg]), to_csubstr(filtered_args_ptr[(size_t)iarg])) << iarg;
    for(size_t iarg = 0; iarg < expected_args.size(); ++iarg)
    {
        EXPECT_EQ(buf_out[iarg].action, expected_args[iarg].action) << iarg;
        EXPECT_EQ(buf_out[iarg].target, expected_args[iarg].target) << iarg;
        EXPECT_EQ(buf_out[iarg].payload, expected_args[iarg].payload) << iarg;
    }
    //
    if(expected_args.size())
    {
        yml::Tree output = yml::parse(reftree);
        Workspace ws(&output);
        ws.apply_opts(buf_out.data(), buf_out.size());
        EXPECT_EQ(yml::emitrs<std::string>(output), yml::emitrs<std::string>(expected_tree));
    }
}

} // namespace conf
} // namespace c4
