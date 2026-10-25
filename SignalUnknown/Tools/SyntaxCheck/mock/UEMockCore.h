// Minimal declarations that imitate the Unreal Engine core API surface used by Signal Unknown.
// ONLY for `clang -fsyntax-only` checks of the project code. This is NOT Unreal Engine and does not
// prove that the project compiles with the real engine. Signatures follow UE 5.x as closely as known.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <type_traits>
#include <utility>
#include <initializer_list>
#include <functional>
#include <memory>
#include <atomic>
#include <limits>

// ------------------------------------------------------------------ basic types
using int8 = signed char;
using uint8 = unsigned char;
using int16 = short;
using uint16 = unsigned short;
using int32 = int;
using uint32 = unsigned int;
using int64 = long long;
using uint64 = unsigned long long;
using SIZE_T = size_t;
using TCHAR = char16_t;
using WIDECHAR = char16_t;
using ANSICHAR = char;
using UTF8CHAR = char8_t;

#define TEXT(x) u##x
#define FORCEINLINE inline
#define FORCENOINLINE
#define SIGNALUNKNOWN_API
#define ENGINE_API
#define CORE_API
#define INDEX_NONE (-1)
#define UE_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define UE_PI (3.1415926535897932f)
#define UE_DOUBLE_PI (3.141592653589793238462643383279502884197169399)
#define UE_TWO_PI (6.28318530717959f)
#define UE_HALF_PI (1.57079632679f)
#define UE_SMALL_NUMBER (1.e-8f)
#define UE_KINDA_SMALL_NUMBER (1.e-4f)
#define UE_BIG_NUMBER (3.4e+38f)
#define check(x) ((void)(x))
#define checkf(x, ...) ((void)(x))
#define checkNoEntry() ((void)0)
#define verify(x) ((void)(x))
#define ensure(x) (!!(x))
#define ensureMsgf(x, ...) (!!(x))
#define UNLIKELY(x) (x)
#define LIKELY(x) (x)
#define PRAGMA_DISABLE_DEPRECATION_WARNINGS
#define PRAGMA_ENABLE_DEPRECATION_WARNINGS

const TCHAR* SUMockToTchar(const char* S);
const TCHAR* SUMockToTchar(const char8_t* S);
const char* SUMockToAnsi(const TCHAR* S);
#define UTF8_TO_TCHAR(x) SUMockToTchar(x)
#define ANSI_TO_TCHAR(x) SUMockToTchar(x)
#define TCHAR_TO_UTF8(x) SUMockToAnsi(x)
#define TCHAR_TO_ANSI(x) SUMockToAnsi(x)

enum EForceInit { ForceInit, ForceInitToZero };
enum ENoInit { NoInit };

template <typename T> struct TNumericLimits
{
	static constexpr T Max() { return std::numeric_limits<T>::max(); }
	static constexpr T Min() { return std::numeric_limits<T>::min(); }
	static constexpr T Lowest() { return std::numeric_limits<T>::lowest(); }
};

namespace ESearchCase { enum Type { CaseSensitive, IgnoreCase }; }
namespace ESearchDir { enum Type { FromStart, FromEnd }; }

// valid printf-style argument (mirrors UE's TIsValidVariadicFunctionArg)
template <typename T>
inline constexpr bool SUIsNarrowString = std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>;
// narrow char* is rejected as well: %s in UE expects TCHAR* (a char* would print garbage)
template <typename T>
inline constexpr bool SUIsValidVarArg = (std::is_arithmetic_v<std::decay_t<T>> || std::is_pointer_v<std::decay_t<T>> || std::is_enum_v<std::decay_t<T>> || std::is_same_v<std::decay_t<T>, std::nullptr_t>) && !SUIsNarrowString<T>;

template <typename FmtType>
inline constexpr bool SUIsTcharLiteral = std::is_array_v<std::remove_reference_t<FmtType>> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<FmtType>>>, TCHAR>;

// ------------------------------------------------------------------ containers
class FDefaultAllocator {};
template <int N> class TInlineAllocator {};
template <typename T, typename Alloc = FDefaultAllocator> class TArray;

template <typename T, typename Alloc>
class TArray
{
public:
	using ElementType = T;
	TArray() = default;
	TArray(std::initializer_list<T> List);
	TArray(const T* Ptr, int32 Count);
	template <typename OtherAlloc, typename = std::enable_if_t<!std::is_same_v<OtherAlloc, Alloc>>> TArray(const TArray<T, OtherAlloc>& Other);

	int32 Num() const;
	bool IsEmpty() const;
	bool IsValidIndex(int32 Index) const;
	int32 Max() const;
	T* GetData();
	const T* GetData() const;
	T& operator[](int32 Index);
	const T& operator[](int32 Index) const;
	T& Last(int32 IndexFromEnd = 0);
	const T& Last(int32 IndexFromEnd = 0) const;
	T& Top();
	T Pop(bool bAllowShrinking = true);
	int32 Add(const T& Item);
	int32 Add(T&& Item);
	int32 AddUnique(const T& Item);
	int32 AddDefaulted(int32 Count = 1);
	T& AddDefaulted_GetRef();
	int32 AddZeroed(int32 Count = 1);
	int32 AddUninitialized(int32 Count = 1);
	template <typename... A> int32 Emplace(A&&... Args);
	template <typename... A> T& Emplace_GetRef(A&&... Args);
	int32 Insert(const T& Item, int32 Index);
	void RemoveAt(int32 Index, int32 Count = 1, bool bAllowShrinking = true);
	void RemoveAtSwap(int32 Index, int32 Count = 1, bool bAllowShrinking = true);
	int32 Remove(const T& Item);
	int32 RemoveSingle(const T& Item);
	int32 RemoveSwap(const T& Item);
	template <typename P> int32 RemoveAll(const P& Pred) { (void)Pred(*begin()); return 0; }
	template <typename K> bool Contains(const K& Item) const;
	template <typename P> bool ContainsByPredicate(P Pred) const { return Pred(*begin()); }
	int32 Find(const T& Item) const;
	bool Find(const T& Item, int32& Index) const;
	template <typename K> int32 IndexOfByKey(const K& Key) const;
	template <typename P> int32 IndexOfByPredicate(P Pred) const { return Pred(*begin()) ? 0 : -1; }
	template <typename K> T* FindByKey(const K& Key);
	template <typename K> const T* FindByKey(const K& Key) const;
	template <typename P> T* FindByPredicate(P Pred) { return Pred(*begin()) ? begin() : nullptr; }
	template <typename P> const T* FindByPredicate(P Pred) const { return Pred(*begin()) ? begin() : nullptr; }
	template <typename P> TArray<T> FilterByPredicate(P Pred) const { (void)Pred(*begin()); return TArray<T>(); }
	void Reset(int32 NewSize = 0);
	void Empty(int32 Slack = 0);
	void SetNum(int32 NewNum, bool bAllowShrinking = true);
	void SetNumZeroed(int32 NewNum, bool bAllowShrinking = true);
	void SetNumUninitialized(int32 NewNum, bool bAllowShrinking = true);
	void Reserve(int32 Number);
	void Init(const T& Element, int32 Number);
	template <typename OtherAlloc> void Append(const TArray<T, OtherAlloc>& Source);
	void Append(std::initializer_list<T> List);
	void Append(const T* Ptr, int32 Count);
	void Sort();
	template <typename P> void Sort(const P& Pred) { (void)Pred(*begin(), *begin()); }
	template <typename P> void StableSort(const P& Pred) { (void)Pred(*begin(), *begin()); }
	void Swap(int32 A, int32 B);
	void Shrink();
	SIZE_T GetAllocatedSize() const;
	T* begin();
	T* end();
	const T* begin() const;
	const T* end() const;
	bool operator==(const TArray& Other) const;
	bool operator!=(const TArray& Other) const;
};

template <typename T> using TArray64 = TArray<T>;
template <typename T> using TArrayView = const TArray<T>&;
template <typename T> using TArrayView64 = const TArray<T>&;

template <typename K, typename V> struct TPair
{
	K Key;
	V Value;
	TPair() = default;
	TPair(const K& InKey, const V& InValue) : Key(InKey), Value(InValue) {}
};
template <typename K, typename V> using TTuple = TPair<K, V>;
template <typename K, typename V> TPair<K, V> MakeTuple(const K& A, const V& B);

template <typename K, typename V>
class TMap
{
public:
	using ElementType = TPair<K, V>;
	V& Add(const K& Key, const V& Value);
	V& Add(const K& Key);
	V& FindOrAdd(const K& Key);
	V& FindOrAdd(const K& Key, const V& Default);
	V* Find(const K& Key);
	const V* Find(const K& Key) const;
	V FindRef(const K& Key) const;
	V& FindChecked(const K& Key);
	const V& FindChecked(const K& Key) const;
	bool Contains(const K& Key) const;
	int32 Remove(const K& Key);
	int32 Num() const;
	bool IsEmpty() const;
	void Reset();
	void Empty(int32 Slack = 0);
	V& operator[](const K& Key);
	const V& operator[](const K& Key) const;
	int32 GenerateKeyArray(TArray<K>& Out) const;
	int32 GenerateValueArray(TArray<V>& Out) const;
	ElementType* begin();
	ElementType* end();
	const ElementType* begin() const;
	const ElementType* end() const;
	struct TIterator
	{
		explicit operator bool() const;
		TIterator& operator++();
		const K& Key() const;
		V& Value() const;
		void RemoveCurrent();
		ElementType* operator->() const;
		ElementType& operator*() const;
	};
	TIterator CreateIterator();
	TIterator CreateConstIterator() const;
	template <typename P> void KeySort(P Pred) {}
	template <typename P> void ValueSort(P Pred) {}
};

template <typename T>
class TSet
{
public:
	TSet() = default;
	TSet(std::initializer_list<T> List);
	void Add(const T& Item, bool* bIsAlreadyInSet = nullptr);
	bool Contains(const T& Item) const;
	int32 Remove(const T& Item);
	int32 Num() const;
	bool IsEmpty() const;
	void Reset();
	void Empty(int32 Slack = 0);
	TArray<T> Array() const;
	void Append(const TArray<T>& Items);
	const T* Find(const T& Item) const;
	T* begin();
	T* end();
	const T* begin() const;
	const T* end() const;
};

template <typename T>
class TOptional
{
public:
	TOptional() = default;
	TOptional(const T& Value);
	bool IsSet() const;
	explicit operator bool() const;
	const T& GetValue() const;
	T& GetValue();
	const T& Get(const T& Default) const;
	void Reset();
	template <typename... A> T& Emplace(A&&... Args);
	TOptional& operator=(const T& Value);
	const T* operator->() const;
	T* operator->();
	const T& operator*() const;
};

template <typename Sig> using TFunction = std::function<Sig>;
template <typename Sig> using TUniqueFunction = std::function<Sig>;
template <typename Sig> using TFunctionRef = std::function<Sig>;

template <typename T>
class TUniquePtr
{
public:
	TUniquePtr() = default;
	TUniquePtr(std::nullptr_t) {}
	explicit TUniquePtr(T* InPtr) : Ptr(InPtr) {}
	TUniquePtr(TUniquePtr&& Other) : Ptr(std::move(Other.Ptr)) {}
	template <typename U> TUniquePtr(TUniquePtr<U>&& Other);
	TUniquePtr& operator=(TUniquePtr&& Other) { Ptr = std::move(Other.Ptr); return *this; }
	TUniquePtr& operator=(std::nullptr_t) { Ptr.reset(); return *this; }
	T* Get() const { return Ptr.get(); }
	T* operator->() const { return Ptr.get(); }
	T& operator*() const { return *Ptr; }
	bool IsValid() const { return Ptr != nullptr; }
	explicit operator bool() const { return Ptr != nullptr; }
	void Reset(T* InPtr = nullptr) { Ptr.reset(InPtr); }
	T* Release() { return Ptr.release(); }
private:
	std::unique_ptr<T> Ptr;
};
template <typename T, typename... A> TUniquePtr<T> MakeUnique(A&&... Args) { return TUniquePtr<T>(new T(std::forward<A>(Args)...)); }

template <typename T> class TSharedRef;
template <typename T>
class TSharedPtr
{
public:
	TSharedPtr() = default;
	TSharedPtr(std::nullptr_t) {}
	template <typename U> TSharedPtr(const TSharedRef<U>& Ref);
	template <typename U> TSharedPtr(const TSharedPtr<U>& Other);
	T* Get() const;
	T* operator->() const;
	T& operator*() const;
	bool IsValid() const;
	explicit operator bool() const;
	void Reset();
	TSharedRef<T> ToSharedRef() const;
};
template <typename T>
class TSharedRef
{
public:
	template <typename U> TSharedRef(const TSharedRef<U>& Other);
	T* operator->() const;
	T& operator*() const;
	T& Get() const;
};
template <typename T> class TWeakPtr
{
public:
	TSharedPtr<T> Pin() const;
	bool IsValid() const;
};
template <typename T, typename... A> TSharedRef<T> MakeShared(A&&... Args);
template <typename T> TSharedPtr<T> MakeShareable(T* Ptr);

template <typename T> T&& MoveTemp(T& V) { return static_cast<T&&>(V); }
template <typename T> T&& MoveTemp(T&& V) { return static_cast<T&&>(V); }
template <typename T> const T& AsConst(T& V) { return V; }
template <typename T> void Swap(T& A, T& B);

uint32 GetTypeHash(int32 V);
uint32 GetTypeHash(uint32 V);
uint32 GetTypeHash(int64 V);
uint32 GetTypeHash(float V);
uint32 GetTypeHash(const void* V);
uint32 HashCombine(uint32 A, uint32 B);
uint32 HashCombineFast(uint32 A, uint32 B);

struct FMemory
{
	static void* Memcpy(void* Dest, const void* Src, SIZE_T Count);
	static void* Memzero(void* Dest, SIZE_T Count);
	static void* Memset(void* Dest, uint8 Char, SIZE_T Count);
	static void* Malloc(SIZE_T Count, uint32 Alignment = 0);
	static void Free(void* Ptr);
};

// ------------------------------------------------------------------ strings
class FString
{
public:
	FString();
	FString(const TCHAR* Str);
	FString(const ANSICHAR* Str);
	FString(int32 Len, const TCHAR* Str);
	FString(const FString&);
	FString(FString&&);
	FString& operator=(const FString&);
	FString& operator=(FString&&);
	FString& operator=(const TCHAR*);

	template <typename FmtType, typename... Types>
	static FString Printf(const FmtType& Fmt, Types... Args)
	{
		static_assert(SUIsTcharLiteral<FmtType>, "FString::Printf: the format must be a TCHAR string literal (TEXT(\"...\")).");
		static_assert((SUIsValidVarArg<Types> && ...), "FString::Printf: invalid argument (pass *FString / *Name.ToString(), not the object).");
		return FString();
	}
	template <typename T> static FString Join(const T& Array, const TCHAR* Separator);
	static FString FromInt(int32 Num);
	static FString SanitizeFloat(double InFloat, int32 InMinFractionalDigits = 1);
	static FString ChrN(int32 NumCharacters, TCHAR Char);

	const TCHAR* operator*() const;
	int32 Len() const;
	bool IsEmpty() const;
	void Reset(int32 NewReservedSize = 0);
	void Empty(int32 Slack = 0);
	void Reserve(int32 CharacterCount);
	TCHAR& operator[](int32 Index);
	const TCHAR& operator[](int32 Index) const;
	FString& operator+=(const FString& Str);
	FString& operator+=(const TCHAR* Str);
	FString& operator+=(TCHAR Char);
	FString& AppendChar(TCHAR Char);
	FString& Append(const FString& Str);
	FString& Append(const TCHAR* Str);
	bool Equals(const FString& Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const;
	bool Contains(const FString& Sub, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type Dir = ESearchDir::FromStart) const;
	bool Contains(const TCHAR* Sub, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type Dir = ESearchDir::FromStart) const;
	bool StartsWith(const FString& Prefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool EndsWith(const FString& Suffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	int32 Find(const FString& Sub, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type Dir = ESearchDir::FromStart, int32 StartPosition = INDEX_NONE) const;
	bool FindChar(TCHAR Char, int32& Index) const;
	bool FindLastChar(TCHAR Char, int32& Index) const;
	FString Left(int32 Count) const;
	FString Right(int32 Count) const;
	FString Mid(int32 Start, int32 Count = 2147483647) const;
	FString LeftChop(int32 Count) const;
	FString RightChop(int32 Count) const;
	void LeftChopInline(int32 Count, bool bAllowShrinking = true);
	void RightChopInline(int32 Count, bool bAllowShrinking = true);
	void LeftInline(int32 Count, bool bAllowShrinking = true);
	FString ToUpper() const;
	FString ToLower() const;
	FString TrimStartAndEnd() const;
	FString TrimStart() const;
	FString TrimEnd() const;
	FString Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	int32 ReplaceInline(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	int32 ParseIntoArray(TArray<FString>& OutArray, const TCHAR* Delim, bool InCullEmpty = true) const;
	int32 ParseIntoArrayLines(TArray<FString>& OutArray, bool InCullEmpty = true) const;
	bool Split(const FString& InS, FString* LeftS, FString* RightS, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart) const;
	bool IsNumeric() const;
	TArray<TCHAR>& GetCharArray();
	const TArray<TCHAR>& GetCharArray() const;

	friend FString operator+(const FString& A, const FString& B);
	friend FString operator+(const FString& A, const TCHAR* B);
	friend FString operator+(const TCHAR* A, const FString& B);
	friend bool operator==(const FString& A, const FString& B);
	friend bool operator==(const FString& A, const TCHAR* B);
	friend bool operator==(const TCHAR* A, const FString& B);
	friend bool operator!=(const FString& A, const FString& B);
	friend bool operator!=(const FString& A, const TCHAR* B);
	friend bool operator<(const FString& A, const FString& B);
};
uint32 GetTypeHash(const FString& S);

struct FCString
{
	static int32 Atoi(const TCHAR* S);
	static float Atof(const TCHAR* S);
	static int32 Strlen(const TCHAR* S);
	static int32 Strcmp(const TCHAR* A, const TCHAR* B);
	static int32 Stricmp(const TCHAR* A, const TCHAR* B);
};

enum EName { NAME_None = 0 };
enum EFindName { FNAME_Find, FNAME_Add };
class FName
{
public:
	FName();
	FName(EName);
	FName(const TCHAR* Name, EFindName FindType = FNAME_Add);
	FName(const ANSICHAR* Name, EFindName FindType = FNAME_Add);
	FString ToString() const;
	void ToString(FString& Out) const;
	bool IsNone() const;
	bool IsValid() const;
	bool operator==(const FName& Other) const;
	bool operator!=(const FName& Other) const;
	bool operator==(EName) const;
	bool operator!=(EName) const;
	bool LexicalLess(const FName& Other) const;
	bool FastLess(const FName& Other) const;
};
uint32 GetTypeHash(const FName& N);

class FText
{
public:
	FText();
	static FText FromString(const FString& S);
	static FText FromString(FString&& S);
	static FText FromName(const FName& N);
	static FText AsCultureInvariant(const FString& S);
	static FText AsNumber(int32 Value);
	static FText AsNumber(float Value);
	static const FText& GetEmpty();
	static FText Format(const FText& Fmt, const FText& A);
	static FText Join(const FText& Delimiter, const TArray<FText>& Args);
	const FString& ToString() const;
	bool IsEmpty() const;
	bool IsEmptyOrWhitespace() const;
	bool EqualTo(const FText& Other) const;
	FText ToUpper() const;
	FText ToLower() const;
};
#define LOCTEXT(Key, Text) FText::FromString(TEXT(Text))
#define NSLOCTEXT(Ns, Key, Text) FText::FromString(TEXT(Text))
#define INVTEXT(Text) FText::FromString(TEXT(Text))

// ------------------------------------------------------------------ logging
namespace ELogVerbosity { enum Type { NoLogging, Fatal, Error, Warning, Display, Log, Verbose, VeryVerbose, All }; }
struct FLogCategoryBase { const TCHAR* Name; };
#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) extern FLogCategoryBase CategoryName;
#define DEFINE_LOG_CATEGORY(CategoryName) FLogCategoryBase CategoryName;
#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity) static FLogCategoryBase CategoryName;
template <typename FmtType, typename... Types>
void SUMockLog(const FLogCategoryBase& Cat, ELogVerbosity::Type V, const FmtType& Fmt, Types... Args)
{
	static_assert(SUIsTcharLiteral<FmtType>, "UE_LOG: the format must be a TCHAR string literal (TEXT(\"...\")).");
	static_assert((SUIsValidVarArg<Types> && ...), "UE_LOG: invalid argument (pass *FString, not the object).");
}
#define UE_LOG(CategoryName, Verbosity, Format, ...) SUMockLog(CategoryName, ELogVerbosity::Verbosity, Format, ##__VA_ARGS__)

// ------------------------------------------------------------------ math
struct FVector2D;
struct FRotator;
struct FQuat;
namespace EAxis { enum Type { None, X, Y, Z }; }

struct FVector
{
	double X = 0, Y = 0, Z = 0;
	static const FVector ZeroVector;
	static const FVector OneVector;
	static const FVector UpVector;
	static const FVector DownVector;
	static const FVector ForwardVector;
	static const FVector BackwardVector;
	static const FVector RightVector;
	static const FVector LeftVector;
	static const FVector XAxisVector;
	static const FVector YAxisVector;
	static const FVector ZAxisVector;
	FVector() = default;
	explicit FVector(EForceInit) {}
	explicit FVector(double InF) : X(InF), Y(InF), Z(InF) {}
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit FVector(const FVector2D& V, double InZ);
	static double DotProduct(const FVector& A, const FVector& B);
	static FVector CrossProduct(const FVector& A, const FVector& B);
	static double Dist(const FVector& A, const FVector& B);
	static double Distance(const FVector& A, const FVector& B);
	static double Dist2D(const FVector& A, const FVector& B);
	static double DistSquared(const FVector& A, const FVector& B);
	static double DistSquared2D(const FVector& A, const FVector& B);
	static double DistXY(const FVector& A, const FVector& B);
	double Size() const;
	double Length() const;
	double Size2D() const;
	double SizeSquared() const;
	double SizeSquared2D() const;
	FVector GetSafeNormal(double Tolerance = UE_SMALL_NUMBER, const FVector& ResultIfZero = FVector(0, 0, 0)) const;
	FVector GetSafeNormal2D(double Tolerance = UE_SMALL_NUMBER, const FVector& ResultIfZero = FVector(0, 0, 0)) const;
	FVector GetUnsafeNormal() const;
	bool Normalize(double Tolerance = UE_SMALL_NUMBER);
	bool IsNearlyZero(double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	bool IsZero() const;
	bool IsNormalized() const;
	bool Equals(const FVector& V, double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	FRotator Rotation() const;
	FRotator ToOrientationRotator() const;
	FQuat ToOrientationQuat() const;
	FVector RotateAngleAxis(double AngleDeg, const FVector& Axis) const;
	double GetMax() const;
	double GetMin() const;
	double GetAbsMax() const;
	FVector GetAbs() const;
	FVector GetClampedToMaxSize(double MaxSize) const;
	FVector GetClampedToSize(double Min, double Max) const;
	FVector ComponentMax(const FVector& Other) const;
	FVector ComponentMin(const FVector& Other) const;
	FVector2D UnitCartesianToSpherical() const;
	FString ToString() const;
	FString ToCompactString() const;
	double& operator[](int32 Index);
	double operator[](int32 Index) const;
	FVector operator+(const FVector& V) const;
	FVector operator-(const FVector& V) const;
	FVector operator-() const;
	FVector operator*(const FVector& V) const;
	FVector operator/(const FVector& V) const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector operator*(T Scale) const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector operator/(T Scale) const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector operator+(T Bias) const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector operator-(T Bias) const;
	FVector& operator+=(const FVector& V);
	FVector& operator-=(const FVector& V);
	FVector& operator*=(const FVector& V);
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector& operator*=(T Scale);
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector& operator/=(T Scale);
	double operator|(const FVector& V) const;
	FVector operator^(const FVector& V) const;
	bool operator==(const FVector& V) const;
	bool operator!=(const FVector& V) const;
};
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector operator*(T Scale, const FVector& V);
uint32 GetTypeHash(const FVector& V);

struct FVector2D
{
	double X = 0, Y = 0;
	static const FVector2D ZeroVector;
	static const FVector2D UnitVector;
	FVector2D() = default;
	explicit FVector2D(EForceInit) {}
	explicit FVector2D(double V) : X(V), Y(V) {}
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	explicit FVector2D(const FVector& V);
	static double Distance(const FVector2D& A, const FVector2D& B);
	static double DistSquared(const FVector2D& A, const FVector2D& B);
	static double DotProduct(const FVector2D& A, const FVector2D& B);
	static double CrossProduct(const FVector2D& A, const FVector2D& B);
	double Size() const;
	double SizeSquared() const;
	double Length() const;
	FVector2D GetSafeNormal(double Tolerance = UE_SMALL_NUMBER) const;
	void Normalize(double Tolerance = UE_SMALL_NUMBER);
	bool IsNearlyZero(double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	bool IsZero() const;
	FVector2D GetRotated(double AngleDeg) const;
	FVector2D GetAbs() const;
	double GetMax() const;
	double GetMin() const;
	FVector2D ClampAxes(double Min, double Max) const;
	FString ToString() const;
	FVector2D operator+(const FVector2D& V) const;
	FVector2D operator-(const FVector2D& V) const;
	FVector2D operator-() const;
	FVector2D operator*(const FVector2D& V) const;
	FVector2D operator/(const FVector2D& V) const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector2D operator*(T Scale) const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector2D operator/(T Scale) const;
	FVector2D& operator+=(const FVector2D& V);
	FVector2D& operator-=(const FVector2D& V);
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector2D& operator*=(T Scale);
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector2D& operator/=(T Scale);
	double operator|(const FVector2D& V) const;
	double operator^(const FVector2D& V) const;
	bool operator==(const FVector2D& V) const;
	bool operator!=(const FVector2D& V) const;
};
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FVector2D operator*(T Scale, const FVector2D& V);
using FVector2f = FVector2D;
using FVector3f = FVector;
using FVector3d = FVector;

struct FVector4
{
	double X = 0, Y = 0, Z = 0, W = 0;
	FVector4() = default;
	FVector4(double InX, double InY, double InZ, double InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
	FVector4(const FVector& V, double InW);
};

struct FIntPoint
{
	int32 X = 0, Y = 0;
	static const FIntPoint ZeroValue;
	static const FIntPoint NoneValue;
	FIntPoint() = default;
	FIntPoint(int32 InX, int32 InY) : X(InX), Y(InY) {}
	explicit FIntPoint(int32 V) : X(V), Y(V) {}
	bool operator==(const FIntPoint& O) const;
	bool operator!=(const FIntPoint& O) const;
	FIntPoint operator+(const FIntPoint& O) const;
	FIntPoint operator-(const FIntPoint& O) const;
	FIntPoint operator*(int32 S) const;
	FIntPoint operator/(int32 S) const;
	int32 Size() const;
};
uint32 GetTypeHash(const FIntPoint& P);
struct FIntVector
{
	int32 X = 0, Y = 0, Z = 0;
	FIntVector() = default;
	FIntVector(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}
};

struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	static const FRotator ZeroRotator;
	FRotator() = default;
	explicit FRotator(EForceInit) {}
	explicit FRotator(double V) : Pitch(V), Yaw(V), Roll(V) {}
	FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	explicit FRotator(const FQuat& Q);
	FVector Vector() const;
	FQuat Quaternion() const;
	FVector Euler() const;
	FVector RotateVector(const FVector& V) const;
	FVector UnrotateVector(const FVector& V) const;
	FRotator GetNormalized() const;
	FRotator GetDenormalized() const;
	FRotator GetInverse() const;
	void Normalize();
	bool IsNearlyZero(double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	bool IsZero() const;
	bool Equals(const FRotator& R, double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	FRotator Clamp() const;
	static double NormalizeAxis(double Angle);
	static double ClampAxis(double Angle);
	static FRotator MakeFromEuler(const FVector& Euler);
	FString ToString() const;
	FString ToCompactString() const;
	FRotator operator+(const FRotator& R) const;
	FRotator operator-(const FRotator& R) const;
	FRotator operator-() const;
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FRotator operator*(T Scale) const;
	FRotator& operator+=(const FRotator& R);
	FRotator& operator-=(const FRotator& R);
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FRotator& operator*=(T Scale);
	bool operator==(const FRotator& R) const;
	bool operator!=(const FRotator& R) const;
};
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> FRotator operator*(T Scale, const FRotator& R);

struct FQuat
{
	double X = 0, Y = 0, Z = 0, W = 1;
	static const FQuat Identity;
	FQuat() = default;
	explicit FQuat(EForceInit) {}
	FQuat(double InX, double InY, double InZ, double InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
	explicit FQuat(const FRotator& R);
	FQuat(const FVector& Axis, double AngleRad);
	FQuat operator*(const FQuat& Q) const;
	FVector operator*(const FVector& V) const;
	FQuat& operator*=(const FQuat& Q);
	FVector RotateVector(const FVector& V) const;
	FVector UnrotateVector(const FVector& V) const;
	FRotator Rotator() const;
	FVector Vector() const;
	FVector Euler() const;
	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;
	FVector GetAxisX() const;
	FVector GetAxisY() const;
	FVector GetAxisZ() const;
	FQuat Inverse() const;
	FQuat GetNormalized(double Tolerance = UE_SMALL_NUMBER) const;
	void Normalize(double Tolerance = UE_SMALL_NUMBER);
	double GetAngle() const;
	bool Equals(const FQuat& Q, double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	static FQuat Slerp(const FQuat& A, const FQuat& B, double Alpha);
	static FQuat MakeFromEuler(const FVector& Euler);
	static FQuat FindBetweenNormals(const FVector& A, const FVector& B);
	static FQuat FindBetweenVectors(const FVector& A, const FVector& B);
};

struct FTransform
{
	static const FTransform Identity;
	FTransform();
	explicit FTransform(const FVector& Translation);
	explicit FTransform(const FRotator& Rotation);
	explicit FTransform(const FQuat& Rotation);
	FTransform(const FRotator& Rotation, const FVector& Translation, const FVector& Scale = FVector(1, 1, 1));
	FTransform(const FQuat& Rotation, const FVector& Translation, const FVector& Scale = FVector(1, 1, 1));
	FVector GetLocation() const;
	FVector GetTranslation() const;
	FQuat GetRotation() const;
	FRotator Rotator() const;
	FVector GetScale3D() const;
	void SetLocation(const FVector& V);
	void SetTranslation(const FVector& V);
	void SetRotation(const FQuat& Q);
	void SetScale3D(const FVector& V);
	void AddToTranslation(const FVector& V);
	FVector TransformPosition(const FVector& V) const;
	FVector TransformPositionNoScale(const FVector& V) const;
	FVector TransformVector(const FVector& V) const;
	FVector TransformVectorNoScale(const FVector& V) const;
	FQuat TransformRotation(const FQuat& Q) const;
	FVector InverseTransformPosition(const FVector& V) const;
	FVector InverseTransformPositionNoScale(const FVector& V) const;
	FVector InverseTransformVector(const FVector& V) const;
	FVector InverseTransformVectorNoScale(const FVector& V) const;
	FQuat InverseTransformRotation(const FQuat& Q) const;
	FVector GetUnitAxis(EAxis::Type Axis) const;
	FVector GetScaledAxis(EAxis::Type Axis) const;
	FTransform Inverse() const;
	FTransform GetRelativeTransform(const FTransform& Other) const;
	bool Equals(const FTransform& Other, double Tolerance = UE_KINDA_SMALL_NUMBER) const;
	FTransform operator*(const FTransform& Other) const;
	FString ToString() const;
};

struct FColor;
struct FLinearColor
{
	float R = 0, G = 0, B = 0, A = 1;
	static const FLinearColor White;
	static const FLinearColor Black;
	static const FLinearColor Red;
	static const FLinearColor Green;
	static const FLinearColor Blue;
	static const FLinearColor Yellow;
	static const FLinearColor Gray;
	static const FLinearColor Transparent;
	FLinearColor() = default;
	explicit FLinearColor(EForceInit) {}
	constexpr FLinearColor(float InR, float InG, float InB, float InA = 1.f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor(const FColor& C);
	explicit FLinearColor(const FVector& V);
	FColor ToFColor(bool bSRGB) const;
	FColor ToFColorSRGB() const;
	FLinearColor CopyWithNewOpacity(float NewOpacity) const;
	FLinearColor LinearRGBToHSV() const;
	FLinearColor HSVToLinearRGB() const;
	float GetLuminance() const;
	float GetMax() const;
	static FLinearColor LerpUsingHSV(const FLinearColor& From, const FLinearColor& To, float Progress);
	static FLinearColor FromSRGBColor(const FColor& C);
	static FLinearColor MakeFromHSV8(uint8 H, uint8 S, uint8 V);
	FLinearColor operator+(const FLinearColor& C) const;
	FLinearColor operator-(const FLinearColor& C) const;
	FLinearColor operator*(const FLinearColor& C) const;
	FLinearColor operator*(float S) const;
	FLinearColor operator/(float S) const;
	FLinearColor& operator+=(const FLinearColor& C);
	FLinearColor& operator*=(float S);
	FLinearColor& operator*=(const FLinearColor& C);
	bool operator==(const FLinearColor& C) const;
	bool operator!=(const FLinearColor& C) const;
	bool Equals(const FLinearColor& C, float Tolerance = UE_KINDA_SMALL_NUMBER) const;
};
FLinearColor operator*(float S, const FLinearColor& C);

struct FColor
{
	uint8 B = 0, G = 0, R = 0, A = 255;
	static const FColor White;
	static const FColor Black;
	static const FColor Red;
	static const FColor Green;
	static const FColor Blue;
	static const FColor Yellow;
	static const FColor Orange;
	static const FColor Silver;
	static const FColor Transparent;
	FColor() = default;
	constexpr FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : B(InB), G(InG), R(InR), A(InA) {}
	explicit FColor(uint32 InColor);
	FLinearColor ReinterpretAsLinear() const;
	uint32 ToPackedARGB() const;
	uint32& DWColor();
	const uint32& DWColor() const;
	bool operator==(const FColor& C) const;
	bool operator!=(const FColor& C) const;
};

struct FBox2D
{
	FVector2D Min, Max;
	bool bIsValid = false;
	FBox2D() = default;
	explicit FBox2D(EForceInit) {}
	FBox2D(const FVector2D& InMin, const FVector2D& InMax);
	bool IsInside(const FVector2D& P) const;
	FVector2D GetCenter() const;
	FVector2D GetExtent() const;
	FVector2D GetSize() const;
	FBox2D& operator+=(const FVector2D& P);
};
struct FBox
{
	FVector Min, Max;
	uint8 IsValid = 0;
	FBox() = default;
	explicit FBox(EForceInit) {}
	FBox(const FVector& InMin, const FVector& InMax);
	bool IsInside(const FVector& P) const;
	bool IsInsideOrOn(const FVector& P) const;
	bool Intersect(const FBox& Other) const;
	FVector GetCenter() const;
	FVector GetExtent() const;
	FVector GetSize() const;
	FBox ExpandBy(double W) const;
	FBox& operator+=(const FVector& P);
	FBox& operator+=(const FBox& B);
	FBox TransformBy(const FTransform& M) const;
	static FBox BuildAABB(const FVector& Origin, const FVector& Extent);
	double ComputeSquaredDistanceToPoint(const FVector& Point) const;
	bool IsInsideXY(const FVector& P) const;
};
struct FSphere
{
	FVector Center;
	double W = 0;
};
struct FPlane
{
	double X = 0, Y = 0, Z = 0, W = 0;
};
struct FMatrix
{
	FRotator Rotator() const;
	FQuat ToQuat() const;
	FVector GetScaledAxis(EAxis::Type Axis) const;
	FVector GetUnitAxis(EAxis::Type Axis) const;
	FVector InverseTransformPosition(const FVector& V) const;
	FVector TransformPosition(const FVector& V) const;
	FVector4 TransformFVector4(const FVector4& V) const;
	FMatrix Inverse() const;
	FMatrix operator*(const FMatrix& Other) const;
};

// FMath: templates as in UE (Min/Max/Clamp need matching types, with float/double mixing allowed like UE5's MIX_FLOATS)
template <typename A, typename B> inline constexpr bool SUFloatMix =
	(std::is_same_v<A, float> && std::is_same_v<B, double>) || (std::is_same_v<A, double> && std::is_same_v<B, float>);

struct FMath
{
	template <class T> static constexpr T Min(const T A, const T B) { return A < B ? A : B; }
	template <class T> static constexpr T Max(const T A, const T B) { return A > B ? A : B; }
	template <class A, class B, typename = std::enable_if_t<SUFloatMix<A, B>>> static constexpr double Min(const A X, const B Y) { return X < Y ? X : Y; }
	template <class A, class B, typename = std::enable_if_t<SUFloatMix<A, B>>> static constexpr double Max(const A X, const B Y) { return X > Y ? X : Y; }
	template <class T> static constexpr T Min3(const T A, const T B, const T C) { return Min(Min(A, B), C); }
	template <class T> static constexpr T Max3(const T A, const T B, const T C) { return Max(Max(A, B), C); }
	template <class T> static constexpr T Clamp(const T X, const T Lo, const T Hi) { return X < Lo ? Lo : (X > Hi ? Hi : X); }
	template <class A, class B, class C, typename = std::enable_if_t<!(std::is_same_v<A, B> && std::is_same_v<B, C>) && std::is_floating_point_v<A> && std::is_floating_point_v<B> && std::is_floating_point_v<C>>>
	static constexpr double Clamp(const A X, const B Lo, const C Hi) { return X < Lo ? Lo : (X > Hi ? Hi : X); }
	template <class T> static constexpr T Abs(const T A) { return A < 0 ? -A : A; }
	template <class T> static constexpr T Sign(const T A) { return A > 0 ? T(1) : (A < 0 ? T(-1) : T(0)); }
	template <class T> static constexpr T Square(const T A) { return A * A; }
	template <class T> static constexpr T Cube(const T A) { return A * A * A; }
	template <class T, class U> static T Lerp(const T& A, const T& B, const U& Alpha);
	template <class T> static T Wrap(const T X, const T Min, const T Max);
	static float Sqrt(float V);
	static double Sqrt(double V);
	static float InvSqrt(float V);
	static float Sin(float V);
	static double Sin(double V);
	static float Cos(float V);
	static double Cos(double V);
	static float Tan(float V);
	static double Tan(double V);
	static float Asin(float V);
	static double Asin(double V);
	static float Acos(float V);
	static double Acos(double V);
	static float Atan(float V);
	static double Atan(double V);
	static float Atan2(float Y, float X);
	static double Atan2(double Y, double X);
	static void SinCos(float* S, float* C, float V);
	static void SinCos(double* S, double* C, double V);
	static float Exp(float V);
	static double Exp(double V);
	static float Exp2(float V);
	static float Loge(float V);
	static double Loge(double V);
	static float LogX(float Base, float V);
	static float Pow(float A, float B);
	static double Pow(double A, double B);
	static float Fmod(float X, float Y);
	static double Fmod(double X, double Y);
	static float Frac(float V);
	static double Frac(double V);
	static float Floor(float V);
	static double Floor(double V);
	static int32 FloorToInt(float V);
	static int32 FloorToInt(double V);
	static int32 FloorToInt32(float V);
	static float FloorToFloat(float V);
	static double FloorToFloat(double V);
	static double FloorToDouble(double V);
	static int32 CeilToInt(float V);
	static int32 CeilToInt(double V);
	static float CeilToFloat(float V);
	static int32 RoundToInt(float V);
	static int32 RoundToInt(double V);
	static float RoundToFloat(float V);
	static double RoundToFloat(double V);
	static double RoundToDouble(double V);
	static int32 TruncToInt(float V);
	static int32 TruncToInt(double V);
	static float TruncToFloat(float V);
	static bool IsNearlyEqual(float A, float B, float Tolerance = UE_SMALL_NUMBER);
	static bool IsNearlyEqual(double A, double B, double Tolerance = UE_SMALL_NUMBER);
	static bool IsNearlyZero(float A, float Tolerance = UE_SMALL_NUMBER);
	static bool IsNearlyZero(double A, double Tolerance = UE_SMALL_NUMBER);
	static bool IsFinite(float A);
	static bool IsFinite(double A);
	static bool IsNaN(float A);
	static bool IsNaN(double A);
	template <class T> static constexpr auto DegreesToRadians(const T& Deg) -> decltype(Deg * (UE_PI / 180.f));
	template <class T> static constexpr auto RadiansToDegrees(const T& Rad) -> decltype(Rad * (180.f / UE_PI));
	static float FRand();
	static float SRand();
	static float FRandRange(float Min, float Max);
	static double FRandRange(double Min, double Max);
	static int32 RandRange(int32 Min, int32 Max);
	static int32 RandHelper(int32 A);
	static bool RandBool();
	static FVector VRand();
	static FVector VRandCone(const FVector& Dir, float ConeHalfAngleRad);
	static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed);
	static double FInterpTo(double Current, double Target, double DeltaTime, double InterpSpeed);
	static float FInterpConstantTo(float Current, float Target, float DeltaTime, float InterpSpeed);
	static double FInterpConstantTo(double Current, double Target, double DeltaTime, double InterpSpeed);
	static FVector VInterpTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed);
	static FVector VInterpConstantTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed);
	static FVector2D Vector2DInterpTo(const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed);
	static FRotator RInterpTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);
	static FRotator RInterpConstantTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);
	static FQuat QInterpTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpSpeed);
	static FLinearColor CInterpTo(const FLinearColor& Current, const FLinearColor& Target, float DeltaTime, float InterpSpeed);
	template <class T> static T InterpEaseInOut(const T& A, const T& B, float Alpha, float Exp);
	template <class T> static T SmoothStep(T A, T B, T X);
	static float ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees);
	static double ClampAngle(double AngleDegrees, double MinAngleDegrees, double MaxAngleDegrees);
	static float UnwindDegrees(float A);
	static double UnwindDegrees(double A);
	static float FindDeltaAngleDegrees(float A1, float A2);
	static float GetMappedRangeValueClamped(const FVector2D& InputRange, const FVector2D& OutputRange, float Value);
	static double GetMappedRangeValueClamped(const FVector2D& InputRange, const FVector2D& OutputRange, double Value);
	static float GetRangePct(float MinValue, float MaxValue, float Value);
	template <class T> static constexpr T DivideAndRoundUp(T Dividend, T Divisor) { return (Dividend + Divisor - 1) / Divisor; }
	template <class T> static constexpr T DivideAndRoundDown(T Dividend, T Divisor) { return Dividend / Divisor; }
	static bool IsPowerOfTwo(uint32 V);
	static FVector ClosestPointOnSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);
	static float PointDistToSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);
	static FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal);
	static float PerlinNoise1D(float Value);
	static float PerlinNoise2D(const FVector2D& Value);
};

// ------------------------------------------------------------------ misc core
enum class EGuidFormats { Digits, DigitsLower, DigitsWithHyphens, DigitsWithHyphensLower, DigitsWithHyphensInBraces, DigitsWithHyphensInParentheses, HexValuesInBraces, UniqueObjectGuid, Short, Base36Encoded };
struct FGuid
{
	uint32 A = 0, B = 0, C = 0, D = 0;
	FGuid() = default;
	FGuid(uint32 InA, uint32 InB, uint32 InC, uint32 InD) : A(InA), B(InB), C(InC), D(InD) {}
	static FGuid NewGuid();
	static bool Parse(const FString& S, FGuid& Out);
	bool IsValid() const;
	void Invalidate();
	FString ToString() const;
	FString ToString(EGuidFormats Format) const;
	bool operator==(const FGuid& O) const;
	bool operator!=(const FGuid& O) const;
	bool operator<(const FGuid& O) const;
};
uint32 GetTypeHash(const FGuid& G);

struct FTimespan
{
	double GetTotalSeconds() const;
	double GetTotalMinutes() const;
	int32 GetHours() const;
	int32 GetMinutes() const;
	static FTimespan FromSeconds(double S);
};
struct FDateTime
{
	FDateTime();
	FDateTime(int32 Year, int32 Month, int32 Day, int32 Hour = 0, int32 Minute = 0, int32 Second = 0, int32 Millisecond = 0);
	static FDateTime Now();
	static FDateTime UtcNow();
	FString ToString() const;
	FString ToString(const TCHAR* Format) const;
	FString ToIso8601() const;
	int64 GetTicks() const;
	int32 GetYear() const;
	int32 GetMonth() const;
	int32 GetDay() const;
	int32 GetHour() const;
	int32 GetMinute() const;
	FTimespan operator-(const FDateTime& O) const;
	bool operator<(const FDateTime& O) const;
	bool operator==(const FDateTime& O) const;
};

struct FRandomStream
{
	FRandomStream();
	explicit FRandomStream(int32 InSeed);
	explicit FRandomStream(FName InName);
	void Initialize(int32 InSeed);
	void Reset() const;
	int32 GetInitialSeed() const;
	int32 GetCurrentSeed() const;
	float GetFraction() const;
	uint32 GetUnsignedInt() const;
	float FRand() const;
	float FRandRange(float Min, float Max) const;
	int32 RandRange(int32 Min, int32 Max) const;
	int32 RandHelper(int32 A) const;
	FVector VRand() const;
	bool RandBool() const;
};

struct FPlatformTime
{
	static double Seconds();
	static uint32 Cycles();
};
struct FPlatformProcess
{
	static void Sleep(float Seconds);
};
struct FPlatformMisc
{
	static void LowLevelOutputDebugString(const TCHAR* S);
};
struct FApp
{
	static double GetDeltaTime();
	static double GetCurrentTime();
	static bool IsGame();
	static const TCHAR* GetProjectName();
	static bool CanEverRender();
};
struct FCommandLine
{
	static const TCHAR* Get();
};
struct FParse
{
	static bool Param(const TCHAR* Stream, const TCHAR* Param);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnSeparator = true);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int32& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, float& Value);
	static bool Bool(const TCHAR* Stream, const TCHAR* Match, bool& OnOff);
};
struct FPaths
{
	static FString ProjectDir();
	static FString ProjectSavedDir();
	static FString ProjectContentDir();
	static FString ProjectConfigDir();
	static FString ProjectUserDir();
	static FString GetPath(const FString& InPath);
	static FString GetCleanFilename(const FString& InPath);
	static FString GetBaseFilename(const FString& InPath, bool bRemovePath = true);
	static FString GetExtension(const FString& InPath, bool bIncludeDot = false);
	static FString ConvertRelativePathToFull(const FString& InPath);
	static bool FileExists(const FString& InPath);
	static bool DirectoryExists(const FString& InPath);
	static void NormalizeFilename(FString& InPath);
	static void NormalizeDirectoryName(FString& InPath);
	static bool MakeStandardFilename(FString& InPath);
	template <typename... T> static FString Combine(T&&... Parts);
};
class IFileManager
{
public:
	static IFileManager& Get();
	virtual ~IFileManager() = default;
	bool MakeDirectory(const TCHAR* Path, bool Tree = false);
	bool DeleteDirectory(const TCHAR* Path, bool RequireExists = false, bool Tree = false);
	bool Delete(const TCHAR* Filename, bool RequireExists = false, bool EvenReadOnly = false, bool Quiet = false);
	bool FileExists(const TCHAR* Filename);
	bool DirectoryExists(const TCHAR* Dirname);
	int64 FileSize(const TCHAR* Filename);
	void FindFiles(TArray<FString>& FileNames, const TCHAR* Filename, bool Files, bool Directories);
	void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension = nullptr);
	bool Move(const TCHAR* Dest, const TCHAR* Src, bool Replace = true, bool EvenIfReadOnly = false, bool Attributes = false, bool bDoNotRetryOrError = false);
	uint32 Copy(const TCHAR* Dest, const TCHAR* Src, bool Replace = true, bool EvenIfReadOnly = false, bool Attributes = false);
};
struct FFileHelper
{
	enum class EEncodingOptions { AutoDetect, ForceAnsi, ForceUnicode, ForceUTF8, ForceUTF8WithoutBOM };
	static bool LoadFileToArray(TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags = 0);
	static bool SaveArrayToFile(const TArray<uint8>& Array, const TCHAR* Filename, IFileManager* FileManager = nullptr, uint32 WriteFlags = 0);
	static bool LoadFileToString(FString& Result, const TCHAR* Filename, uint32 VerifyFlags = 0, uint32 ReadFlags = 0);
	static bool SaveStringToFile(const FString& String, const TCHAR* Filename, EEncodingOptions EncodingOptions = EEncodingOptions::AutoDetect, IFileManager* FileManager = nullptr, uint32 WriteFlags = 0);
};

class FCriticalSection
{
public:
	void Lock();
	void Unlock();
};
class FScopeLock
{
public:
	explicit FScopeLock(FCriticalSection* InSync);
	~FScopeLock();
};

enum class EAsyncExecution { TaskGraph, TaskGraphMainThread, Thread, ThreadIfForkSafe, ThreadPool, LargeThreadPool };
namespace ENamedThreads { enum Type { AnyThread, GameThread, ActualRenderingThread, AnyBackgroundThreadNormalTask, AnyHiPriThreadNormalTask }; }
template <typename R> class TFuture
{
public:
	bool IsReady() const;
	R Get() const;
	void Wait() const;
};
template <typename F> auto Async(EAsyncExecution Execution, F&& Function, TUniqueFunction<void()> CompletionCallback = nullptr) -> TFuture<decltype(Function())> { TUniqueFunction<decltype(Function())()> Stored(Function); return {}; }
void AsyncTask(ENamedThreads::Type Thread, TUniqueFunction<void()> Function);
bool IsInGameThread();

// ------------------------------------------------------------------ delegates
class FDelegateHandle
{
public:
	bool IsValid() const;
	void Reset();
	bool operator==(const FDelegateHandle& O) const;
};

template <typename Sig> class TDelegate;
template <typename R, typename... A>
class TDelegate<R(A...)>
{
public:
	TDelegate() = default;
	template <typename F> static TDelegate CreateLambda(F&& Func) { TFunction<R(A...)> Stored(Func); return TDelegate(); }
	template <typename U> static TDelegate CreateUObject(U* Obj, R (U::*Func)(A...));
	template <typename U> static TDelegate CreateUObject(const U* Obj, R (U::*Func)(A...) const);
	template <typename U> static TDelegate CreateRaw(U* Obj, R (U::*Func)(A...));
	template <typename U> static TDelegate CreateWeakLambda(U* Obj, TFunction<R(A...)> Func);
	static TDelegate CreateStatic(R (*Func)(A...));
	template <typename F> void BindLambda(F&& Func) { TFunction<R(A...)> Stored(Func); }
	template <typename U> void BindUObject(U* Obj, R (U::*Func)(A...));
	template <typename U> void BindRaw(U* Obj, R (U::*Func)(A...));
	void BindStatic(R (*Func)(A...));
	bool IsBound() const;
	void Unbind();
	R Execute(A... Args) const;
	bool ExecuteIfBound(A... Args) const;
};

template <typename... A>
class TMulticastDelegate
{
public:
	template <typename F> FDelegateHandle AddLambda(F&& Func) { TFunction<void(A...)> Stored(Func); return FDelegateHandle(); }
	template <typename U> FDelegateHandle AddUObject(U* Obj, void (U::*Func)(A...));
	template <typename U> FDelegateHandle AddUObject(const U* Obj, void (U::*Func)(A...) const);
	template <typename U> FDelegateHandle AddRaw(U* Obj, void (U::*Func)(A...));
	template <typename U, typename F> FDelegateHandle AddWeakLambda(U* Obj, F&& Func) { TFunction<void(A...)> Stored(Func); return FDelegateHandle(); }
	FDelegateHandle AddStatic(void (*Func)(A...));
	FDelegateHandle Add(const TDelegate<void(A...)>& D);
	bool Remove(FDelegateHandle Handle);
	template <typename U> void RemoveAll(const U* Obj);
	void Clear();
	bool IsBound() const;
	void Broadcast(A... Args) const;
};

// dynamic delegates: the handler must be a member function with exactly these parameters (UFUNCTION)
template <typename... A>
class TDynamicMulticastDelegate
{
public:
	template <typename U> void __Internal_AddDynamic(U* Obj, void (U::*Func)(A...), FName FuncName);
	template <typename U> void __Internal_AddUniqueDynamic(U* Obj, void (U::*Func)(A...), FName FuncName);
	template <typename U> void __Internal_RemoveDynamic(U* Obj, void (U::*Func)(A...), FName FuncName);
	bool IsBound() const;
	void Clear();
	void Broadcast(A... Args) const;
};
template <typename... A>
class TDynamicDelegate
{
public:
	template <typename U> void __Internal_BindDynamic(U* Obj, void (U::*Func)(A...), FName FuncName);
	bool IsBound() const;
	void ExecuteIfBound(A... Args) const;
};
#define AddDynamic(UserObject, FuncName) __Internal_AddDynamic(UserObject, FuncName, FName())
#define AddUniqueDynamic(UserObject, FuncName) __Internal_AddUniqueDynamic(UserObject, FuncName, FName())
#define RemoveDynamic(UserObject, FuncName) __Internal_RemoveDynamic(UserObject, FuncName, FName())
#define BindDynamic(UserObject, FuncName) __Internal_BindDynamic(UserObject, FuncName, FName())

#define DECLARE_DELEGATE(N) using N = TDelegate<void()>;
#define DECLARE_DELEGATE_OneParam(N, T1) using N = TDelegate<void(T1)>;
#define DECLARE_DELEGATE_TwoParams(N, T1, T2) using N = TDelegate<void(T1, T2)>;
#define DECLARE_DELEGATE_RetVal(R, N) using N = TDelegate<R()>;
#define DECLARE_MULTICAST_DELEGATE(N) using N = TMulticastDelegate<>;
#define DECLARE_MULTICAST_DELEGATE_OneParam(N, T1) using N = TMulticastDelegate<T1>;
#define DECLARE_MULTICAST_DELEGATE_TwoParams(N, T1, T2) using N = TMulticastDelegate<T1, T2>;
#define DECLARE_MULTICAST_DELEGATE_ThreeParams(N, T1, T2, T3) using N = TMulticastDelegate<T1, T2, T3>;
#define DECLARE_MULTICAST_DELEGATE_FourParams(N, T1, T2, T3, T4) using N = TMulticastDelegate<T1, T2, T3, T4>;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(N) using N = TDynamicMulticastDelegate<>;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(N, T1, P1) using N = TDynamicMulticastDelegate<T1>;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(N, T1, P1, T2, P2) using N = TDynamicMulticastDelegate<T1, T2>;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(N, T1, P1, T2, P2, T3, P3) using N = TDynamicMulticastDelegate<T1, T2, T3>;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(N, T1, P1, T2, P2, T3, P3, T4, P4) using N = TDynamicMulticastDelegate<T1, T2, T3, T4>;
#define DECLARE_DYNAMIC_DELEGATE(N) using N = TDynamicDelegate<>;
#define DECLARE_DYNAMIC_DELEGATE_OneParam(N, T1, P1) using N = TDynamicDelegate<T1>;
#define DECLARE_EVENT(Owner, N) using N = TMulticastDelegate<>;
using FSimpleDelegate = TDelegate<void()>;
using FSimpleMulticastDelegate = TMulticastDelegate<>;
