#pragma once
#include <cstdint>
#include <system_error>

enum AppStatus : int32_t {
	SUC = 0,
	FAIL = -1
};


inline std::error_code MakeErrorCode(const AppStatus err, const std::error_category& cate){
    return std::error_code(static_cast<int>(err), cate);
}

inline std::error_code MakeGenerateError(const AppStatus err){
    return MakeErrorCode(err, std::generic_category());
}

inline std::error_code MakeSystemError(const AppStatus err){
    return MakeErrorCode(err, std::system_category());
}