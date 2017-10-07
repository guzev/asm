//
// Created by Ivan Guzev on 6.10.17.
//

#include <sys/mman.h>
#include <iostream>
#include <vector>


using namespace std;

template<typename ... Args>
struct args;

template<>
struct args<> {
    static const int INT = 0;
    static const int SSE = 0;
};

template<typename Any, typename ... Other>
struct args<Any, Other ...> {
    static const int INT = args<Other ...>::INT + 1;
    static const int SSE = args<Other ...>::SSE;
};

template<typename ... Other>
struct args<double, Other ...> {
    static const int INT = args<Other ...>::INT;
    static const int SSE = args<Other ...>::SSE + 1;
};

template<typename ... Other>
struct args<float, Other ...> {
    static const int INT = args<Other ...>::INT;
    static const int SSE = args<Other ...>::SSE + 1;
};


template<typename T>
struct trampoline;

template<typename R, typename... Args>
void swap(trampoline<R(Args...)> &a, trampoline<R(Args...)> &b);

template<typename T, typename ... Args>
struct trampoline<T(Args ...)> {
private:
    static const int REGISTERS = 6;
    static const int BYTE = 8;

    const char *shifts[REGISTERS] = {
            "\x48\x89\xfe" /*mov rsi rdi*/, "\x48\x89\xf2" /*mov rdx rsi*/,
            "\x48\x89\xd1" /*mov rcx rdx*/, "\x49\x89\xc8" /*mov r8 rcx*/,
            "\x4d\x89\xc1" /*mov r9 r8*/, "\x41\x51" /*push r9*/
    };

    void add(char *&p, const char *command) {
        for (const char *i = command; *i; i++)
            *(p++) = *i;
    }

    void add(char *&p, const char *command, int32_t c) {
        add(p, command);
        *(int32_t *) p = c;
        p += BYTE / 2;
    }

    void add(char *&p, const char *command, void *c) {
        add(p, command);
        *(void **) p = c;
        p += BYTE;
    }

    void *address = nullptr;
    const size_t PAGE_SIZE = 4096;
    const size_t SIZE_PER_FUNC = 255;

    void* allocate_page() {
        char* page = static_cast<char*>(mmap(nullptr, PAGE_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (page == MAP_FAILED)
            return nullptr;
        *page = 0;
        page += sizeof(int);
        for (size_t i = 0; i < PAGE_SIZE - sizeof(int); i += SIZE_PER_FUNC)
            *reinterpret_cast<char**>(page + i - (i == 0 ? 0 : SIZE_PER_FUNC)) = page + i;
        return page;
    }

    bool counter_add(void *page, int delta) {
        void *start_of_page = (void*)(((size_t)page >> 12) << 12);
        *static_cast<int*>(start_of_page) += delta;
        if (*static_cast<int*>(start_of_page) == 0) {
            munmap(start_of_page, PAGE_SIZE);
            return false;
        }
        return true;
    }

    void* my_malloc() {
        if (!address) {
            address = allocate_page();
            if (!address) {
                return nullptr;
            }
        }
        counter_add(address, 1);
        void *ret = address;
        address = *reinterpret_cast<void**>(address);
        return ret;
    }

    template<typename Function>
    static T caller(void *obj, Args ...args) {
        return (*static_cast<Function *>(obj))(std::forward<Args>(args)...);
    }

    template<typename Function>
    static void del(void *func) {
        delete static_cast<Function *>(func);
    }

    void *func;
    void *code;
    void (*deleter)(void *);

public:
    template<typename Function>
    trampoline(Function f) : func(new Function(std::move(f))), deleter(del<Function>) {
        code = my_malloc();
        char *pointer_to_code = (char *) code;

        if (args<Args ...>::INT >= REGISTERS) {
            int stack_size = BYTE * (args<Args ...>::INT - 5 + std::max(args<Args ...>::SSE - BYTE, 0));
            //move r11 [rsp]
            add(pointer_to_code, "\x4c\x8b\x1c\x24");
            for (int i = 5; i >= 0; i--)
                add(pointer_to_code, shifts[i]);

            //mov rax,[rsp]
            add(pointer_to_code, "\x48\x89\xe0");
            //add rax [stack_size + 8]
            add(pointer_to_code, "\x48\x05", stack_size);
            //add rsp 8
            add(pointer_to_code, "\x48\x81\xc4", BYTE);

            char *label_1 = pointer_to_code;

            //cmp rax rsp
            add(pointer_to_code, "\x48\x39\xe0");
            //je
            add(pointer_to_code, "\x74");

            char *label_2 = pointer_to_code;
            pointer_to_code++;

            // add rsp 8
            add(pointer_to_code, "\x48\x81\xc4\x08");
            pointer_to_code += 3;
            // mov rdi [rsp]
            add(pointer_to_code, "\x48\x8b\x3c\x24");
            // mov [rsp - 8] rdi
            add(pointer_to_code, "\x48\x89\x7c\x24\xf8");
            // jmp
            add(pointer_to_code, "\xeb");

            *pointer_to_code = label_1 - pointer_to_code - 1;
            pointer_to_code++;
            *label_2 = pointer_to_code - label_2 - 1;

            //mov [rsp], r11
            add(pointer_to_code, "\x4c\x89\x1c\x24");
            //sub rsp, stack_size
            add(pointer_to_code, "\x48\x81\xec", stack_size);
            //mov rdi, imm
            add(pointer_to_code, "\x48\xbf", func);
            //mov rax, imm
            add(pointer_to_code, "\x48\xb8", (void *) &caller<Function>);
            //call rax
            add(pointer_to_code, "\xff\xd0");
            // pop r9
            add(pointer_to_code, "\x41\x59");
            // mov r11 [rsp + stack_size]
            add(pointer_to_code, "\x4c\x8b\x9c\x24", stack_size - BYTE);
            // mov [rsp] r11
            add(pointer_to_code, "\x4c\x89\x1c\x24\xc3");
        } else {
            for (int i = args<Args ...>::INT - 1; i >= 0; i--)
                add(pointer_to_code, shifts[i]);
            //mov rdi imm
            add(pointer_to_code, "\x48\xbf", func);
            //mov rax imm
            add(pointer_to_code, "\x48\xb8", (void *) &caller<Function>);
            //mov jmp raxi
            add(pointer_to_code, "\xff\xe0");
        }
    }

    trampoline(trampoline &&other) : code(other.code), deleter(other.deleter), func(other.func) {
        other.func = nullptr;
    }

    trampoline(const trampoline &) = delete;

    template<class TRAMPOLINE>
    trampoline &operator=(TRAMPOLINE &&function) {
        trampoline tmp(std::move(function));
        swap(*this, tmp);
        return *this;
    }

    T (*get() const )(Args ... args) {
        return (T(*)(Args ... args))code;
    }

    void swap(trampoline &other) {
        ::swap(*this, other);
    }

    friend void::swap<>(trampoline &a, trampoline &b);

    ~trampoline() {
        if (func)
            deleter(func);
        *(void **) code = address;
        address = (void **) code;
    }
};

template<typename R, typename... Args>
void swap(trampoline<R(Args...)> &a, trampoline<R(Args...)> &b) {
    std::swap(a.func, b.func);
    std::swap(a.code, b.code);
    std::swap(a.deleter, b.deleter);
}

struct functional_object {
    int operator()(int c1, int c2, int c3, int c4, int c5) {
        cout << "in functional_object()\n";
        return 2345619;
    }
    ~functional_object() {
        cout << "functional_object deleted\n";
    }
};

void test1() {
    int b = 123;
    functional_object fo;

    trampoline<int (int, int, int, int, int)> tr(fo);
    auto p = tr.get();
    p(1, 2, 3, 4, 5);
    b = 124;

    int new_i = 999;
    int res = p(2, 3, 4, 5, 6);
    cout << res << "\n";
    cout << b << " " << new_i << "\n";
}

void test2() {
    int b = 124;
    trampoline<long long (int, int, int, int, int, int, int, int, int)>
            tr([&](int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8, int c9) {
                   cout << c1 << " " << c2 << " " << c3 << " " << c4 << " " << c5 << " " << c6 << " ";
                   cout << c7 << " " << c8 << " " << c9 << "|\n";
                   return 0;
               }
    );
    auto p = tr.get();
    {
        int res = p(100, 200, 300, 400, 500, 600, 700, 800, 900);
        cout << "result : " << res << "\n";
    }

    b = 125;

    p(9, 8, 7, 6, 5, 4, 3, 2, 1);
}



int main() {
    test1();
    test2();
    return 0;
}
