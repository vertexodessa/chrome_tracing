#include "tracing.h"

#include <vector>
#include <thread>

void f(){
    
};

int main() {
    using namespace std;
    vector<thread> v;
    for(int i=0; i < 5; ++i) {
        v.emplace_back(thread(f));
    }

    for(int i=0; i < 5; ++i) {
        v[i].join();
    }
    return 0;
}
