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
    {"-n", "--node", "set node value", 2, Opt::set_node},
    {"-f", "--file", "load file"     , 1, Opt::load_file},
    {"-d", "--dir" , "load dir"      , 1, Opt::load_dir},
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

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
    test_opts({"-a", "-n", "key1.key1val0[1]", "here it is", "-b", "b0", "--node", "key1.key1val1", "now this is a scalar", "-c", "c0", "c1", "-n", "key1.key1val0[1]", "here it is overrided"},
              {"-a",                                         "-b", "b0",                                                    "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

struct casefiles
{
    casefiles()
    {
        fs::mkdir("somedir");
        fs::file_put_contents("somedir/file0", csubstr(R"(
key0:
  key0val0: now replaced as a scalar
)"));
        fs::file_put_contents("somedir/file2", csubstr(R"(
key0:
  key0val0: now replaced as a scalar, v2
)"));
        fs::file_put_contents("somedir/file3", csubstr(R"(
key1:
  key1val0: this one too, v2
)"));
        fs::file_put_contents("somedir/file1", csubstr(R"(
key1:
  key1val0: this one too
)"));
    }
    ~casefiles()
    {
        fs::rmfile("somedir/file3");
        fs::rmfile("somedir/file2");
        fs::rmfile("somedir/file1");
        fs::rmfile("somedir/file0");
        fs::rmdir("somedir");
    }
};

TEST(opt, load_file)
{
    casefiles setup;
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::load_file, {}, "somedir/file0"},
        {Opt::load_file, {}, "somedir/file1"},
    };
    expected_tree["key0"]["key0val0"].clear_children();
    expected_tree["key0"]["key0val0"].set_type(yml::KEYVAL);
    expected_tree["key0"]["key0val0"].set_val("now replaced as a scalar");
    expected_tree["key1"]["key1val0"].clear_children();
    expected_tree["key1"]["key1val0"].set_type(yml::KEYVAL);
    expected_tree["key1"]["key1val0"].set_val("this one too");
    test_opts({"-a", "-f", "somedir/file0", "-b", "b0", "--file", "somedir/file1", "-c", "c0", "c1"},
              {"-a",                        "-b", "b0",                            "-c", "c0", "c1"},
              expected_args,
              expected_tree);
}

TEST(opt, load_dir)
{
    casefiles setup;
    yml::Tree expected_tree = yml::parse(reftree);
    OptArg expected_args[] = {
        {Opt::load_dir, {}, "somedir"},
    };
    expected_tree["key0"]["key0val0"].clear_children();
    expected_tree["key0"]["key0val0"].set_type(yml::KEYVAL);
    expected_tree["key0"]["key0val0"].set_val("now replaced as a scalar, v2");
    expected_tree["key1"]["key1val0"].clear_children();
    expected_tree["key1"]["key1val0"].set_type(yml::KEYVAL);
    expected_tree["key1"]["key1val0"].set_val("this one too, v2");
    test_opts({"-a", "-d", "somedir", "-b", "b0", "-c", "c0", "c1"},
              {"-a",                  "-b", "b0", "-c", "c0", "c1"},
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
    std::vector<char*> input_args_ptr, filtered_args_ptr;
    int argc;
    char **argv;
    auto reset_args = [&]{
        to_args(input_args, &input_args_ptr);
        to_args(filtered_args, &filtered_args_ptr);
        argc = (int) input_args_ptr.size();
        argv = input_args_ptr.data();
    };
    auto check_args = [&]{
        for(int iarg = 0; iarg < argc; ++iarg)
            EXPECT_EQ(to_csubstr(input_args_ptr[iarg]), to_csubstr(filtered_args_ptr[iarg])) << iarg;
    };
    // must accept nullptr
    reset_args();
    size_t ret = parse_opts(&argc, &argv, nullptr);
    ASSERT_NE(ret, argerror);
    EXPECT_EQ(ret, expected_args.size());
    check_args();
    // must deal with insufficient buffer size
    reset_args();
    std::vector<OptArg> buf;
    buf.resize(expected_args.size() / 2);
    span<OptArg> buf_out = {buf.data(), buf.size()};
    ret = parse_opts(&argc, &argv, &buf_out);
    ASSERT_NE(ret, argerror);
    EXPECT_EQ(ret, expected_args.size());
    EXPECT_EQ(argc, (int)filtered_args.size());
    EXPECT_EQ(buf_out.size(), buf.size());
    EXPECT_EQ(buf_out.data(), buf.data());
    check_args();
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
    check_args();
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
