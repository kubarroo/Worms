#include "AppException.h"
#include <sstream>


AppException::AppException(int line, const char* file, const char* message) noexcept
    : line(line), file(file), message(message)
{
}

const char* AppException::what() const noexcept
{
    std::ostringstream oss;
    oss << GetType() << std::endl << GetOriginString() << std::endl << message << std::endl;
    whatBuffer = oss.str();
    return whatBuffer.c_str();
}

const char* AppException::GetType() const noexcept
{
    return "Worms Exception";
}

int AppException::GetLine() const noexcept
{
    return line;
}

const std::string& AppException::GetFile() const noexcept
{
    return file;
}

std::string AppException::GetOriginString() const noexcept
{
    std::ostringstream oss;
    oss << "[File] " << file << std::endl << "[Line] " << line;
    return oss.str();
}
