#ifdef _MSC_VER
#  include <nmmintrin.h>
#  if defined(_M_X64)
#    define TI_POPCNT_U64 _mm_popcnt_u64
#  endif
#  define TI_POPCNT_U32 _mm_popcnt_u32
#elif defined(__POPCNT__)
#  include <popcntintrin.h>
#  if defined(__x86_64__)
#    define TI_POPCNT_U64 __builtin_popcountll
#  endif
#  define TI_POPCNT_U32 __builtin_popcount
#else
#  error "__POPCNT__ is not defined by compiler"
#endif

int main()
{
#ifdef TI_POPCNT_U64
    int i = TI_POPCNT_U64(1);
#endif
    int j = TI_POPCNT_U32(1);
    return 0;
}
