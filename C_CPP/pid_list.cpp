#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#define SIG_UPDATE __SIGRTMIN + 16
// DIR 路径
#define DIR_PATH "/proc/"
//要解析的文件名
#define FILE_NAME "stat"

struct PIDInfo {
  int pid;
  char PIDName[32];
};  // pid号和程序名存储结构体

struct PID_DirNode {
  struct dirent *rent;       //进程目录信息
  struct PID_DirNode *next;  //下一个结点
};

struct PID_DirList {
  int num;                   //解析/proc目录下文件(目录的个数)
  struct PID_DirNode *head;  //存储链表的头结点
};

/*初始化链表*/
struct PID_DirList *init_PID_DirList() {
  struct PID_DirList *list =
      (struct PID_DirList *)malloc(sizeof(struct PID_DirList));
  list->num = 0;
  list->head = NULL;
  return list;
}

/*链表操作:增加结点到链表*/
void add_NodeToList(struct PID_DirList **list, struct dirent *rent) {
  if (rent == NULL) {
    return;
  }
  if (atoi(rent->d_name) <= 0)  //不是数字的，一般是系统的东西，不做介绍
  {
    printf("%s\n", rent->d_name);
    return;
  }
  /*头插单链表，注意，还要给目录信息指针分配空间*/
  struct PID_DirNode *new_DirNode =
      (struct PID_DirNode *)malloc(sizeof(struct PID_DirNode));
  new_DirNode->rent = (struct dirent *)malloc(sizeof(struct dirent));
  memcpy(new_DirNode->rent, rent, sizeof(struct dirent));
  new_DirNode->next = (*list)->head;
  (*list)->head = new_DirNode;
  (*list)->num++;
  return;
}

/*链表操作:打印链表信息*/
void printPID_DirList(struct PID_DirList *list) {
  while (list->num--) {
    printf("PID File : %s\n", list->head->rent->d_name);  // NOTE 会改变原本的链表
    list->head = list->head->next;
  }
  return;
}

/*链表操作:销毁链表*/
void Destray_PID_DirList(struct PID_DirList **pid_DirList) {
  struct PID_DirNode *tmpFreeNode = (*pid_DirList)->head;
  while ((*pid_DirList)->num--) {
    tmpFreeNode = (*pid_DirList)->head;
    if (tmpFreeNode) {
      free(tmpFreeNode);             // NOTE  没有释放rent分配的空间
      (*pid_DirList)->head = (*pid_DirList)->head->next;
    }
  }
  free((*pid_DirList));
  return;
}

/*=======================================
                读目录操作:解析/proc目录下的文件(目录),添加到链表中去
=====================================*/
int addDirNameToList(struct PID_DirList *pid_DirList) {
  // struct statstruct_proc s_proc;
  struct dirent *tmp_rent = NULL;
  if (pid_DirList == NULL) {
    return 0;
  }
  DIR *PID_FileDir;
  /*打开目录*/
  if ((PID_FileDir = opendir(DIR_PATH)) == NULL) {
    perror("open dir error ::");
    exit(1);
  }
  /*循环读，直至读完整个目录*/
  while ((tmp_rent = readdir(PID_FileDir)) != NULL) {
    if (strcmp(tmp_rent->d_name, ".") == 0 ||
        strcmp(tmp_rent->d_name, "..") == 0) {
      continue;
    }
    if (tmp_rent->d_type == DT_DIR) {
      //如果是目录，就加入链表，后面做解析
      add_NodeToList(&pid_DirList, tmp_rent);
    }
  }
  closedir(PID_FileDir);
  return 1;
}
/*
        通过应用程序名字在链表中查找出对应的PID号
*/
int findPIDByNameFromList(struct PID_DirList *pid_DirList, char *processName) {
  char AbsPathFileName[64] = {0};    // stat文件的绝对路径
  char readFile_Buffer[2048] = {0};  //读取文件信息存储buff
  int FileFD = -1;
  int ret = -1;
  while (pid_DirList->num--) {
    memset(AbsPathFileName, 0, sizeof(AbsPathFileName));
    snprintf(AbsPathFileName, sizeof(AbsPathFileName), "%s%s/%s", DIR_PATH,
             pid_DirList->head->rent->d_name, FILE_NAME);
    FileFD = open(AbsPathFileName, O_RDONLY);  //只读打开
    if (FileFD < 0) {                          //打开错误，换一个目录
      if (pid_DirList->head != NULL) {
        pid_DirList->head = pid_DirList->head->next;
      }
      printf("--name=%s--\n", AbsPathFileName);
      perror("open file fail");
      continue;
    }
    //读取文件信息
    ret = read(FileFD, readFile_Buffer, sizeof(readFile_Buffer));
    if (ret < 0) {
      printf("--fd=%d--\n", FileFD);
      perror("read data fail");
      close(FileED);
      continue;
    }
    close(FileFD);
    char *bufferPtr = (char *)readFile_Buffer;  //指针操作
    char *subStr = NULL;
    int sepStep = 0;  //代表是第几个单词
    struct PIDInfo pidInfo;
    memset(&pidInfo, 0, sizeof(struct PIDInfo));
    /*
            strsep函数，是拆分字符串。
            参数1:字符串的二级指针，
            参数2:要拆分的字符串，这里我们需要通过空格(" ")把字符串拆开
            比如：buff = "hello world" --> subStr = strsep(&bufferPtr," ")
            -->执行strsep后，subStr = "hello" ,buff = "world"

    */
    while ((subStr = strsep(&bufferPtr, " ")) != NULL) {
      sepStep++;
      if (sepStep == 1) {  //第一个单词是PID号,转成int
        pidInfo.pid = atoi(subStr);
      } else if (sepStep == 2) {  //应用程序名字
        strncpy(pidInfo.PIDName, subStr, sizeof(pidInfo.PIDName));
        break;
      }
    }
    /*比较解析出来的应用程序名字和需要查找的应用程序名字是否一样，一样则返回PID号*/
    if (strncmp(pidInfo.PIDName, processName, strlen(processName)) == 0) {
      return pidInfo.pid;
    }

    if (pid_DirList->head != NULL) {
      pid_DirList->head = pid_DirList->head->next;
    } else {
      break;
    }
  }
  return -1;
}

int main(int argc, char *argv[]) {
  struct PID_DirList *pid_DirList = init_PID_DirList();
	if(pid_DirList == NULL)
	{
		return 1;	
	}
	//解析/proc目录
	if(addDirNameToList(pid_DirList))
	{
		printf("add PID Dir Success!!\n");
	}
	//查找应用程序的PID
	int f_pid = findPIDByNameFromList(pid_DirList,"(tcpsvd)");
	printf("find signals pid = %d\n",f_pid);
	Destray_PID_DirList(&pid_DirList);

  // kill 对应的进程
	// int ret = kill(f_pid,SIG_UPDATE);
	// if(ret == -1)
	// {
	// 	perror("kill error");
	// }
	return 0;
}
