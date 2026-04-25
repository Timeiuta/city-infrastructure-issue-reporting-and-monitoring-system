#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define MAX 256

typedef struct {
    int id;
    char inspector[50];
    float latitude;
    float longitude;
    char category[20];
    int severity;
    time_t timestamp;
    char description[100];
} Report;

/* ================= PERMISSION ================= */
int check_permission(const char *path, const char *role, int mode){
    struct stat st;
    if(stat(path,&st) < 0){
        perror("stat");
        return 0;
    }

    if(strcmp(role,"manager")==0){
        if(mode == R_OK && !(st.st_mode & S_IRUSR)) return 0;
        if(mode == W_OK && !(st.st_mode & S_IWUSR)) return 0;
    } else {
        if(mode == R_OK && !(st.st_mode & S_IRGRP)) return 0;
        if(mode == W_OK && !(st.st_mode & S_IWGRP)) return 0;
    }

    return 1;
}

/* ================= PERM STRING ================= */
void perm_to_str(mode_t mode, char *str){
    char chars[] = {'r','w','x'};
    for(int i=0;i<9;i++){
        str[i] = (mode & (1 << (8-i))) ? chars[i%3] : '-';
    }
    str[9]='\0';
}

/* ================= SYMLINK CHECK ================= */
void check_symlink(char *name){
    struct stat st;
    if(lstat(name,&st)==0 && S_ISLNK(st.st_mode)){
        char target[MAX];
        int len = readlink(name,target,sizeof(target)-1);
        if(len>=0){
            target[len]='\0';
            if(stat(target,&st)<0){
                printf("Dangling link: %s\n",name);
            }
        }
    }
}

/* ================= LOG ================= */
void log_action(char *district, char *role, char *user, char *action){
    if(strcmp(role,"manager")!=0){
        printf("Only manager can write logs!\n");
        return;
    }

    char path[MAX];
    sprintf(path,"%s/logged_district",district);

    if(!check_permission(path,role,W_OK)){
        printf("No permission for log!\n");
        return;
    }

    int fd = open(path,O_WRONLY|O_APPEND);
    if(fd<0) return;

    char buf[MAX];
    sprintf(buf,"%ld %s %s %s\n",time(NULL),role,user,action);
    write(fd,buf,strlen(buf));
    close(fd);
}

/* ================= CREATE ================= */
void create_district(char *name){
    mkdir(name,0750);

    char path[MAX];

    sprintf(path,"%s/reports.dat",name);
    int fd=open(path,O_CREAT|O_RDWR,0664);
    close(fd);

    sprintf(path,"%s/district.cfg",name);
    fd=open(path,O_CREAT|O_RDWR,0640);
    write(fd,"threshold=2\n",12);
    close(fd);

    sprintf(path,"%s/logged_district",name);
    fd=open(path,O_CREAT|O_RDWR,0644);
    close(fd);

    char linkname[MAX];
    sprintf(linkname,"active_reports-%s",name);
    sprintf(path,"%s/reports.dat",name);
    symlink(path,linkname);
}

/* ================= ADD ================= */
void add_report(char *district, char *user, char *role){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    if(!check_permission(path,role,W_OK)){
        printf("No write permission!\n");
        return;
    }

    int fd=open(path,O_WRONLY|O_APPEND);

    Report r;
    r.id=rand()%10000;
    strcpy(r.inspector,user);
    r.latitude=45;
    r.longitude=21;
    strcpy(r.category,"road");
    r.severity=2;
    r.timestamp=time(NULL);
    strcpy(r.description,"Sample");

    write(fd,&r,sizeof(r));
    close(fd);
}

/* ================= LIST ================= */
void list_reports(char *district, char *role){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    if(!check_permission(path,role,R_OK)){
        printf("No read permission!\n");
        return;
    }

    struct stat st;
    stat(path,&st);

    char perm[10];
    perm_to_str(st.st_mode,perm);

    printf("Perm:%s Size:%ld Last:%ld\n\n",perm,st.st_size,st.st_mtime);

    int fd=open(path,O_RDONLY);
    Report r;

    while(read(fd,&r,sizeof(r))>0){
        printf("ID:%d %s %s %d\n",r.id,r.inspector,r.category,r.severity);
    }

    close(fd);
}

/* ================= VIEW ================= */
void view_report(char *district,int id){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    int fd=open(path,O_RDONLY);
    Report r;

    while(read(fd,&r,sizeof(r))>0){
        if(r.id==id){
            printf("ID:%d\nInspector:%s\nCategory:%s\nSeverity:%d\nDesc:%s\n",
                   r.id,r.inspector,r.category,r.severity,r.description);
        }
    }

    close(fd);
}

/* ================= REMOVE ================= */
void remove_report(char *district,int id){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    int fd=open(path,O_RDWR);
    Report r;

    while(read(fd,&r,sizeof(r))>0){
        if(r.id==id){
            Report temp;
            off_t pos=lseek(fd,0,SEEK_CUR);

            while(read(fd,&temp,sizeof(temp))>0){
                lseek(fd,pos-sizeof(r),SEEK_SET);
                write(fd,&temp,sizeof(temp));
                pos+=sizeof(temp);
            }

            off_t size=lseek(fd,0,SEEK_END);
            ftruncate(fd,size-sizeof(r));
            break;
        }
    }

    close(fd);
}

/* ================= UPDATE ================= */
void update_threshold(char *district,int val,char *role){
    if(strcmp(role,"manager")!=0){
        printf("Only manager!\n");
        return;
    }

    char path[MAX];
    sprintf(path,"%s/district.cfg",district);

    struct stat st;
    stat(path,&st);

    if((st.st_mode & 0777)!=0640){
        printf("Permission changed!\n");
        return;
    }

    int fd=open(path,O_WRONLY|O_TRUNC);
    char buf[50];
    sprintf(buf,"threshold=%d\n",val);
    write(fd,buf,strlen(buf));
    close(fd);
}

/* ================= FILTER ================= */
int parse_condition(const char *input,char *field,char *op,char *value){
    sscanf(input,"%[^:]:%[^:]:%s",field,op,value);
    return 1;
}

int match_condition(Report *r,const char *field,const char *op,const char *value){
    if(strcmp(field,"severity")==0){
        int v=atoi(value);
        if(strcmp(op,">=")==0) return r->severity>=v;
        if(strcmp(op,"<=")==0) return r->severity<=v;
        if(strcmp(op,">")==0) return r->severity>v;
        if(strcmp(op,"<")==0) return r->severity<v;
        if(strcmp(op,"==")==0) return r->severity==v;
        if(strcmp(op,"!=")==0) return r->severity!=v;
    }

    if(strcmp(field,"category")==0){
        if(strcmp(op,"==")==0) return strcmp(r->category,value)==0;
        if(strcmp(op,"!=")==0) return strcmp(r->category,value)!=0;
    }

    if(strcmp(field,"inspector")==0){
        if(strcmp(op,"==")==0) return strcmp(r->inspector,value)==0;
    }

    if(strcmp(field,"timestamp")==0){
        int v=atoi(value);
        if(strcmp(op,">=")==0) return r->timestamp>=v;
        if(strcmp(op,"<=")==0) return r->timestamp<=v;
        if(strcmp(op,">")==0) return r->timestamp>v;
        if(strcmp(op,"<")==0) return r->timestamp<v;
        if(strcmp(op,"==")==0) return r->timestamp==v;
        if(strcmp(op,"!=")==0) return r->timestamp!=v;
}

    return 0;
}

void filter_reports(char *district,int count,char *conds[]){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    int fd=open(path,O_RDONLY);
    Report r;

    while(read(fd,&r,sizeof(r))>0){
        int ok=1;

        for(int i=0;i<count;i++){
            char f[50],o[10],v[50];
            parse_condition(conds[i],f,o,v);

            if(!match_condition(&r,f,o,v)){
                ok=0;
                break;
            }
        }

        if(ok){
            printf("ID:%d %s %s %d\n",r.id,r.inspector,r.category,r.severity);
        }
    }

    close(fd);
}

/* ================= MAIN ================= */
int main(int argc,char *argv[]){
    char *role=NULL,*user=NULL,*cmd=NULL,*district=NULL;
    int id=0,val=0;

    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--role")==0) role=argv[++i];
        else if(strcmp(argv[i],"--user")==0) user=argv[++i];
        else if(strcmp(argv[i],"--add")==0){ cmd="add"; district=argv[++i]; }
        else if(strcmp(argv[i],"--list")==0){ cmd="list"; district=argv[++i]; }
        else if(strcmp(argv[i],"--view")==0){ cmd="view"; district=argv[++i]; id=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--remove_report")==0){ cmd="remove"; district=argv[++i]; id=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--update_threshold")==0){ cmd="update"; district=argv[++i]; val=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--filter")==0){ cmd="filter"; district=argv[++i]; }
    }

    if(strcmp(cmd,"add")==0){
        create_district(district);
        add_report(district,user,role);
        log_action(district,role,user,"ADD");
    }

    else if(strcmp(cmd,"list")==0){
        list_reports(district,role);
    }

    else if(strcmp(cmd,"view")==0){
        view_report(district,id);
    }

    else if(strcmp(cmd,"remove")==0){
        if(strcmp(role,"manager")!=0){
            printf("Only manager!\n");
            return 0;
        }
        remove_report(district,id);
        log_action(district,role,user,"REMOVE");
    }

    else if(strcmp(cmd,"update")==0){
        update_threshold(district,val,role);
    }

    else if(strcmp(cmd,"filter")==0){
        filter_reports(district,argc-4,&argv[4]);
    }

    return 0;
}