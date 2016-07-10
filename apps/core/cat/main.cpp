#include <cstdio>
using namespace std;

#include "coreutils.h"



static void print_version() {
    
}

static void print_help() {
    
}


bool show_all = false;

extern "C" int main(int argc, char** argv) {
    ARG_BEGIN();
    ARG("v", "--version", false, []() { print_version(); });
    ARG("h", "--help", false, []() { print_help(); });
    ARG("A", "--show-all", false, []() { show_all = true; });
    ARG_END(argv);
}