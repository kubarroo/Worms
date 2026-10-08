#pragma once
#include <exception>
#include <string>

class AppException : public std::exception
{
public:
	AppException( int line, const char* file, const char* message ) noexcept;
	virtual const char* what() const noexcept override;
	virtual const char* GetType() const noexcept;

	int GetLine() const noexcept;
	const std::string& GetFile() const noexcept;
	std::string GetOriginString() const noexcept;
protected:
	int line;
	std::string file;
	std::string message;
	mutable std::string whatBuffer;
};

#define APP_THROW(message) throw AppException(__LINE__, __FILE__, message)