#include <cstdio>
struct Node { long long val; Node* nxt; };
int main() {
    const int n = 50000;
    Node* head = new Node{0, nullptr};
    Node* cur = head;
    for (int i = 1; i < n; i++) { cur->nxt = new Node{i, nullptr}; cur = cur->nxt; }
    long long total = 0;
    for (cur = head; cur; cur = cur->nxt) total += cur->val;
    printf("%lld\n", total);
}
