#pragma once
#include "AppException.h"

class ECS_Exception : public AppException
{
public:
    ECS_Exception(int line, const char* file, const char* message) noexcept
        : AppException(line, file, message)
    {
    }

    virtual inline const char* GetType() const noexcept override
    {
        return "ECS Exception";
    }
};

#define ECS_THROW(message) throw ECS_Exception(__LINE__, __FILE__, message)