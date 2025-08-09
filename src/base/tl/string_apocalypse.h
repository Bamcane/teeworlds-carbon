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
		str_copy(buffer, str, length + 1);
	}

	string(const char *str, int given_length)
	{
		if(!str)
			str = "";
		length = given_length;
		buffer = new char[length + 1];
		str_copy(buffer, str, length + 1);
		buffer[length] = '\0';
	}

	string(const string &other)
	{
		length = other.length;
		buffer = new char[length + 1];
		str_copy(buffer, other.buffer, length + 1);
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
		str_copy(newBuffer, other.buffer, other.length + 1);

		delete[] buffer;

		buffer = newBuffer;
		length = other.length;

		return *this;
	}

	string operator+(const string &other) const
	{
		string result;
		delete[] result.buffer;
		result.length = this->length + other.length;
		result.buffer = new char[result.length + 1];

		str_copy(result.buffer, this->buffer, this->length + 1);
		str_append(result.buffer, other.buffer, result.length + 1);

		return result;
	}

	string &operator+=(const string &other)
	{
		if(this == &other || other.length == 0)
			return *this;

		size_t newLength = length + other.length;
		char *newBuffer = new char[newLength + 1];

		str_copy(newBuffer, buffer, length + 1);
		str_append(newBuffer, other.buffer, newLength + 1);

		delete[] buffer;

		buffer = newBuffer;
		length = newLength;

		return *this;
	}

	size_t size() const { return length; }
	operator const char *() const { return buffer; }
	const char *c_str() const { return buffer; }
	bool operator<(const char *other_str) const { return str_comp(buffer, other_str) < 0; }
};

#endif // BASE_TL_STRING_APOCALYPSE_H
