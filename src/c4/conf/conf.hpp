#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>

namespace c4 {
namespace yml {
class Tree;
} // namespace yml

namespace conf {

struct Workspace
{
    yml::Tree   m_tree_ws;
    yml::Tree * m_tree;
    yml::Tree * m_output;
    bool        m_load_started;
    csubstr     m_arena_when_load_started;

    Workspace(yml::Tree *output, yml::Tree *t=nullptr);

    void prepare_add_file(const char *filename);
    void prepare_add_conf(csubstr tree_path_eq_conf_yml);
    void prepare_add_conf(csubstr tree_path, csubstr conf_yml);

    bool add_file(const char *filename);
    void add_conf(csubstr tree_path_eq_conf_yml);
    void add_conf(csubstr tree_path, csubstr conf_yml);

private:

    void _load_started();
    substr _alloc_arena(size_t sz) const;
};



} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
