#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)
// OK
#else
#error "C++20 is not supported"
#endif

#include <vector>

int main()
{
    vector<int> data{11, 22, 33}; 
    sort(data);
    return 0;
}
