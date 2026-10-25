// Mock of the UObject system (syntax checks only, see UEMockCore.h).
#pragma once
#include "UEMockCore.h"

#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UINTERFACE(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UMETA(...)
#define UPARAM(...)
#define UDELEGATE(...)
#define SU_BODY_COMBINE_INNER(A, B, C, D) A##B##C##D
#define SU_BODY_COMBINE(A, B, C, D) SU_BODY_COMBINE_INNER(A, B, C, D)
#define GENERATED_BODY(...) SU_BODY_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY)
#define GENERATED_UCLASS_BODY(...) GENERATED_BODY()
#define GENERATED_USTRUCT_BODY(...) GENERATED_BODY()

class UObject;
class UClass;
class UWorld;
class UPackage;
class UScriptStruct;
class FObjectInstancingGraph;

enum EObjectFlags { RF_NoFlags = 0, RF_Public = 1, RF_Standalone = 2, RF_Transactional = 8, RF_Transient = 64, RF_ClassDefaultObject = 16 };
inline EObjectFlags operator|(EObjectFlags A, EObjectFlags B) { return EObjectFlags(int(A) | int(B)); }

class FObjectInitializer
{
public:
	template <class T> FObjectInitializer& SetDefaultSubobjectClass(FName Name);
	template <class T> FObjectInitializer& DoNotCreateDefaultSubobject(FName Name);
	UObject* GetObj() const;
};

class FArchive;
class FOutputDevice;
struct FPropertyChangedEvent;

class UObject
{
public:
	typedef UObject ThisClass;
	UObject();
	UObject(const FObjectInitializer& ObjectInitializer);
	virtual ~UObject();
	static UClass* StaticClass();
	virtual UWorld* GetWorld() const;
	FString GetName() const;
	FName GetFName() const;
	FString GetPathName() const;
	UClass* GetClass() const;
	UObject* GetOuter() const;
	template <class T> T* GetTypedOuter() const;
	UPackage* GetOutermost() const;
	template <class T> bool IsA() const;
	bool IsA(const UClass* C) const;
	template <class T> bool Implements() const;
	bool IsValidLowLevel() const;
	bool IsTemplate() const;
	bool HasAnyFlags(EObjectFlags Flags) const;
	void SetFlags(EObjectFlags Flags);
	void ClearFlags(EObjectFlags Flags);
	void AddToRoot();
	void RemoveFromRoot();
	void MarkAsGarbage();
	void ConditionalBeginDestroy();
	bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, uint32 Flags = 0);
	virtual void PostInitProperties();
	virtual void PostLoad();
	virtual void BeginDestroy();
	virtual void FinishDestroy();
	virtual void Serialize(FArchive& Ar);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& E);
	virtual bool IsSupportedForNetworking() const;
	void SaveConfig(uint64 Flags = 0, const TCHAR* Filename = nullptr);
	void LoadConfig(UClass* ConfigClass = nullptr, const TCHAR* Filename = nullptr);
	void TryUpdateDefaultConfigFile(const FString& SpecificFileLocation = FString(), bool bWarnIfFail = true);
	template <class T> T* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false);
	UObject* GetDefaultSubobjectByName(FName ToFind);
	virtual void GetLifetimeReplicatedProps(TArray<struct FLifetimeProperty>& OutLifetimeProps) const;
	uint32 GetUniqueID() const;
};

class UField : public UObject {};
class UStruct : public UField {};
class UClass : public UStruct
{
public:
	UObject* GetDefaultObject(bool bCreateIfNeeded = true) const;
	template <class T> T* GetDefaultObject() const;
	bool IsChildOf(const UClass* SomeBase) const;
	template <class T> bool IsChildOf() const;
	FString GetName() const;
};
class UScriptStruct : public UStruct {};
class UEnum : public UField
{
public:
	FString GetNameStringByValue(int64 Value) const;
	FText GetDisplayNameTextByValue(int64 Value) const;
};
template <typename T> UEnum* StaticEnum();
template <typename T> UScriptStruct* StaticStruct();
class UInterface : public UObject
{
public:
	static UClass* StaticClass();
};
class UPackage : public UObject {};
UPackage* GetTransientPackage();

// ------------------------------------------------------------------ object pointers
template <typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;
	TObjectPtr(std::nullptr_t) {}
	TObjectPtr(T* InPtr);
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TObjectPtr(const TObjectPtr<U>& Other);
	TObjectPtr& operator=(T* InPtr);
	TObjectPtr& operator=(std::nullptr_t);
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TObjectPtr& operator=(const TObjectPtr<U>& Other);
	T* Get() const;
	operator T*() const;
	T* operator->() const;
	T& operator*() const;
};
template <typename T> uint32 GetTypeHash(const TObjectPtr<T>& P);

template <typename T>
class TWeakObjectPtr
{
public:
	TWeakObjectPtr() = default;
	TWeakObjectPtr(std::nullptr_t) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr(U* Object);
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr(const TObjectPtr<U>& Object);
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr(const TWeakObjectPtr<U>& Other);
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr& operator=(U* Object);
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr& operator=(const TObjectPtr<U>& Object);
	T* Get() const;
	T* Get(bool bEvenIfPendingKill) const;
	bool IsValid() const;
	bool IsStale() const;
	void Reset();
	T* operator->() const;
	T& operator*() const;
	explicit operator bool() const;
	bool operator==(const TWeakObjectPtr& Other) const;
	bool operator!=(const TWeakObjectPtr& Other) const;
	template <typename U> bool operator==(const U* Other) const;
	template <typename U> bool operator!=(const U* Other) const;
};
template <typename T> uint32 GetTypeHash(const TWeakObjectPtr<T>& P);

class FSoftObjectPath
{
public:
	FSoftObjectPath();
	FSoftObjectPath(const FString& Path);
	FSoftObjectPath(const TCHAR* Path);
	bool IsValid() const;
	bool IsNull() const;
	UObject* TryLoad() const;
	UObject* ResolveObject() const;
	FString ToString() const;
};
template <typename T>
class TSoftObjectPtr
{
public:
	TSoftObjectPtr() = default;
	TSoftObjectPtr(const FSoftObjectPath& Path);
	TSoftObjectPtr(T* Object);
	bool IsNull() const;
	bool IsValid() const;
	bool IsPending() const;
	T* Get() const;
	T* LoadSynchronous() const;
	FSoftObjectPath ToSoftObjectPath() const;
	FString ToString() const;
};
template <typename T>
class TSoftClassPtr
{
public:
	bool IsNull() const;
	UClass* LoadSynchronous() const;
	UClass* Get() const;
};

template <typename T>
class TSubclassOf
{
public:
	TSubclassOf() = default;
	TSubclassOf(std::nullptr_t) {}
	TSubclassOf(UClass* C);
	UClass* Get() const;
	operator UClass*() const;
	UClass* operator->() const;
	T* GetDefaultObject() const;
	explicit operator bool() const;
};

template <typename T>
class TStrongObjectPtr
{
public:
	TStrongObjectPtr() = default;
	explicit TStrongObjectPtr(T* Object);
	T* Get() const;
	T* operator->() const;
	bool IsValid() const;
	void Reset(T* NewObject = nullptr);
	explicit operator bool() const;
};

template <typename T> struct TIsUObjectPtr : std::false_type {};

template <class To, class From> To* Cast(From* Src) { return dynamic_cast<To*>(Src); }
template <class To, class From> const To* Cast(const From* Src) { return dynamic_cast<const To*>(Src); }
template <class To, class From> To* Cast(const TObjectPtr<From>& Src) { return dynamic_cast<To*>(Src.Get()); }
template <class To, class From> To* CastChecked(From* Src) { return static_cast<To*>(Src); }
template <class To, class From> To* CastChecked(const TObjectPtr<From>& Src) { return static_cast<To*>(Src.Get()); }
template <class To, class From> To* ExactCast(From* Src) { return dynamic_cast<To*>(Src); }
bool IsValid(const UObject* Test);
template <class T> bool IsValid(const TObjectPtr<T>& Test) { return Test.Get() != nullptr; }

template <class T> T* NewObject(UObject* Outer = (UObject*)GetTransientPackage());
template <class T> T* NewObject(UObject* Outer, FName Name, EObjectFlags Flags = RF_NoFlags, UObject* Template = nullptr, bool bCopyTransientsFromClassDefaults = false, FObjectInstancingGraph* InInstanceGraph = nullptr);
template <class T> T* NewObject(UObject* Outer, const UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags, UObject* Template = nullptr, bool bCopyTransientsFromClassDefaults = false, FObjectInstancingGraph* InInstanceGraph = nullptr);
template <class T> const T* GetDefault();
template <class T> const T* GetDefault(UClass* Class);
template <class T> T* GetMutableDefault();
template <class T> T* LoadObject(UObject* Outer, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = 0, UPackage* Sandbox = nullptr);
UObject* StaticLoadObject(UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = 0);
template <class T> T* FindObject(UObject* Outer, const TCHAR* Name, bool ExactClass = false);
template <class T> T* DuplicateObject(T const* SourceObject, UObject* Outer, const FName Name = NAME_None);

template <class T>
class TObjectIterator
{
public:
	TObjectIterator();
	explicit operator bool() const;
	TObjectIterator& operator++();
	T* operator*() const;
	T* operator->() const;
};

template <typename E> class TEnumAsByte
{
public:
	TEnumAsByte() = default;
	TEnumAsByte(E InValue);
	operator E() const;
	E GetValue() const;
	TEnumAsByte& operator=(E InValue);
};

// reflection-only base types used by USTRUCTs
struct FTableRowBase
{
	virtual ~FTableRowBase() = default;
};
