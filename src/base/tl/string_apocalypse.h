#ifndef BASE_TL_STRING_APOCALYPSE_H
#define BASE_TL_STRING_APOCALYPSE_H

#include <cstdio>
#include <cstring>

class string
{
private:
	char *buffer;
	size_t length;

public:
	string(const char *str = "")
	{
		if(!str)
			str = "";
		length = strlen(str);
		buffer = new char[length + 1];
		strncpy(buffer, str, length);
		buffer[length] = '\0';
	}

	string(const char *str, int given_length)
	{
		if(!str)
			str = "";
		length = given_length;
		buffer = new char[length + 1];
		strncpy(buffer, str, length);
		buffer[length] = '\0';
	}

	string(const string &other)
	{
		length = other.length;
		buffer = new char[length + 1];
		strncpy(buffer, other.buffer, length);
		buffer[length] = '\0';
	}

	~string()
	{
		delete[] buffer;
	}

	string &operator=(const string &other)
	{
		if(this == &other)
			return *this;

		char *newBuffer = new char[other.length + 1];
		strncpy(newBuffer, other.buffer, other.length);

		delete[] buffer;

		buffer = newBuffer;
		length = other.length;

		return *this;
	}

	string operator+(const string &other) const
	{
		string result;
		result.length = this->length + other.length;
		result.buffer = new char[result.length + 1];
		strcpy(result.buffer, this->buffer);
		strcat(result.buffer, other.buffer);
		return result;
	}

	string &operator+=(const string &other)
	{
		if(this == &other || other.length == 0)
			return *this;

		size_t newLength = length + other.length;
		char *newBuffer = new char[newLength + 1];

		strcpy(newBuffer, buffer);
		strcat(newBuffer, other.buffer);

		delete[] buffer;

		buffer = newBuffer;
		length = newLength;

		return *this;
	}

	size_t size() const { return length; }
	operator const char *() const { return buffer; }
	const char *c_str() const { return buffer; }
};

#endif // BASE_TL_STRING_APOCALYPSE_H
