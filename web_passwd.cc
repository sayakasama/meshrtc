#include <stdio.h>
#include <stdlib.h>
#include <string.h>


////////////////////////////////////////////////////////////////////////////////
char char_mapping(char original_char) 
{
	char out_char;
	
	if((original_char < 'z') && (original_char >= 'a')) 
	{
		out_char = original_char + 1;
	}
/*	else if(original_char == 'z')
	{
	    out_char = 'a';
	} 
	else if(original_char == 'Z')
	{
	    out_char = 'A';
	} 
	else if((original_char < 'Z') && (original_char >= 'A')) 
	{
		out_char = original_char + 1;
	}
*/	
	else 
	{
		out_char = original_char;
	}
	
    return out_char;  
}

void web_passwd_calc(const char* in_str, char *out_str) 
{
	int i;
	
    if (out_str == NULL) 
	{
		return;
	}

	i = 0;
    while(in_str[i] != 0)
	{
        out_str[i] = char_mapping(in_str[i]);
		i++;
    }
	out_str[i] = 0;

    return;
}
////////////////////////////////////////////////////////////////////////////////
#ifdef __WEB_SELF_TEST__

// 定义映射表结构体
typedef struct {
    char original;
    char mapped;
} CharMapping;

// 定义映射表，每个字母映射到它的下一个字母
CharMapping charMapping[] = {
    {'a', 'b'},
    {'b', 'c'},
    {'c', 'd'},
    {'d', 'e'},
    {'e', 'f'},
    {'f', 'g'},
    {'g', 'h'},
    {'h', 'i'},
    {'i', 'j'},
    {'j', 'k'},
    {'k', 'l'},
    {'l', 'm'},
    {'m', 'n'},
    {'n', 'o'},
    {'o', 'p'},
    {'p', 'q'},
    {'q', 'r'},
    {'r', 's'},
    {'s', 't'},
    {'t', 'u'},
    {'u', 'v'},
    {'v', 'w'},
    {'w', 'x'},
    {'x', 'y'},
    {'y', 'z'},
    // 添加其他字符的映射规则
};

////////////////////////////////////////////////////////////////////////
// 获取映射后的字符
char getMappedChar(char originalChar) {
    for (size_t i = 0; i < sizeof(charMapping) / sizeof(CharMapping); ++i) {
        if (charMapping[i].original == originalChar) {
            return charMapping[i].mapped;
        }
    }
    return originalChar;  // 如果找不到映射，默认返回原字符
}

// 对字符串进行字符调换
char* customSwapCharacters(const char* inputStr) {
    size_t inputLen = strlen(inputStr);
    char* swappedStr = (char*)malloc((inputLen + 1) * sizeof(char));

    if (swappedStr != NULL) {
        for (size_t i = 0; i < inputLen; ++i) {
            swappedStr[i] = getMappedChar(inputStr[i]);
        }

        swappedStr[inputLen] = '\0';  // 添加字符串结尾的 null 字符
    }

    return swappedStr;
}

////////////////////////////////////////////////////////////////////////
int main(int argc , char *argv[]) 
{
    // 例子
    const char* inputString = "abcde";
	char buf[128];
	
	if(argc > 1)
	{
		inputString = argv[1];
	}
	
    char* swappedResult = customSwapCharacters(inputString);
    if (swappedResult != NULL) 
	{
        printf("Original: %s\n", inputString);
        printf("Swapped: %s\n", swappedResult);

        free(swappedResult);  // 释放内存
    }

	str_swap(inputString, buf);
    printf("++++ Original: %s\n", inputString);
    printf("++++ Swapped: %s\n", buf);
		
    return 0;
}
#endif