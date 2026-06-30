#include <cstdio>
struct Node { int v; Node *l, *r; };
Node* make_tree(int depth) {
    Node* n = new Node{1, nullptr, nullptr};
    if (depth > 0) { n->l = make_tree(depth-1); n->r = make_tree(depth-1); }
    return n;
}
long long check_tree(Node* n) {
    if (!n->l) return 1;
    return 1 + check_tree(n->l) + check_tree(n->r);
}
void free_tree(Node* n) { if (!n) return; free_tree(n->l); free_tree(n->r); delete n; }
int main() {
    int max_depth = 12;
    long long total = 0;
    for (int d = 4; d <= max_depth; d += 2) {
        int iters = 1 << (max_depth - d + 4);
        for (int i = 0; i < iters; i++) {
            Node* t = make_tree(d);
            total += check_tree(t);
            free_tree(t);
        }
    }
    printf("%lld\n", total);
}
