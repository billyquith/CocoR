#if !defined(COCO_STRINGBUILDER_H__)
#define COCO_STRINGBUILDER_H__

#include<stddef.h>

namespace Coco {

class StringBuilder  
{
public:
	StringBuilder(int capacity = 32);
	StringBuilder(const char *val);
	
	virtual ~StringBuilder();
	void Append(const char val);
	void Append(const char *val);
	char* ToString();
	int GetLength() { return length; };

private:
	void Init(int capacity);
	char *data;
	int capacity;
	int length;
};

}; // namespace

#endif // !defined(COCO_STRINGBUILDER_H__)
