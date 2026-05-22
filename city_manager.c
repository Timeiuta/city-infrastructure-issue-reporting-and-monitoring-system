#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>

#define MAX 256




typedef struct {
    int id;                // 4 bytes
    int severity;          // 4 bytes
    float latitude;        // 4 bytes
    float longitude;       // 4 bytes
    time_t timestamp;      // 8 bytes
    char inspector[52];    // 52 bytes
    char category[24];     // 24 bytes
    char description[108]; // 108 bytes
} Report;                  // Total size = Exactly 208 bytes                // Total size = 208 bytes (Perfect alignment!)

void notify_monitor(char *district, char *role, char *user);
void create_district(char *name);

/* ================= PERM STRING ================= */
void perm_to_str(mode_t mode, char *str){
    str[0] = (mode & S_IRUSR) ? 'r' : '-';
    str[1] = (mode & S_IWUSR) ? 'w' : '-';
    str[2] = (mode & S_IXUSR) ? 'x' : '-';
    str[3] = (mode & S_IRGRP) ? 'r' : '-';
    str[4] = (mode & S_IWGRP) ? 'w' : '-';
    str[5] = (mode & S_IXGRP) ? 'x' : '-';
    str[6] = (mode & S_IROTH) ? 'r' : '-';
    str[7] = (mode & S_IWOTH) ? 'w' : '-';
    str[8] = (mode & S_IXOTH) ? 'x' : '-';
    str[9] = '\0';
}


/* ================= PERMISSION ================= */

int check_permission(const char *path, const char *role, int need_read, int need_write){
    struct stat st;

    if(stat(path, &st) < 0){
        perror("stat");
        return 1;
    }

    if(strcmp(role, "manager") == 0){
        if(need_read && !(st.st_mode & S_IRUSR)) return 0;
        if(need_write && !(st.st_mode & S_IWUSR)) return 0;
    }
    else if(strcmp(role, "inspector") == 0){
        if(need_read && !(st.st_mode & S_IRGRP)) return 0;
        if(need_write && !(st.st_mode & S_IWGRP)) return 0;
    }
    else{
        return 0;
    }

    return 1;
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
    char path[MAX];
    sprintf(path,"%s/logged_district",district);

    if(!check_permission(path,role,0, 1)){
        printf("No permission for log!\n");
        return;
    }

    int fd = open(path,O_WRONLY|O_APPEND | O_CREAT, 0644);


    if(fd<0) return;

    char buf[MAX*2];
    sprintf(buf,"%ld %s %s %s\n",time(NULL),role,user,action);
    write(fd,buf,strlen(buf));
    close(fd);
}

/* ================= CREATE DISTRICT================= */
void create_district(char *name){
    mkdir(name,0750);
    chmod(name, 0750);

    char path[MAX];

    sprintf(path,"%s/reports.dat",name);
    int fd=open(path,O_CREAT|O_RDWR,0664);
    close(fd);
    chmod(path, 0664);

    sprintf(path,"%s/district.cfg",name);
    fd=open(path,O_CREAT|O_RDWR,0640);
    write(fd,"threshold=2\n",12);
    close(fd);
    chmod(path, 0640);

    sprintf(path,"%s/logged_district",name);
    fd=open(path,O_CREAT|O_RDWR,0660);
    close(fd);
    chmod(path, 0660);

    char linkname[MAX];
    sprintf(linkname,"active_reports-%s",name);
    sprintf(path,"%s/reports.dat",name);
    unlink(linkname);
    symlink(path,linkname);
}

/* ================= ADD REPORT================= */
void add_report(
    char *district,
    char *user,
    char *role,
    char *category,
    int severity,
    float lat,
    float lon,
    char *desc
){
    char path[MAX];

    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if(!check_permission(path, role, 0,1)){
        printf("No write permission!\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_APPEND);

    if(fd < 0){
        perror("open reports.dat failed");
        return;
    }

    struct stat st;

    if(stat(path, &st) < 0){
        perror("Stat failed");
        close(fd);
        return;
    }

    Report r;

    r.id =(st.st_size / sizeof(Report)) + 1;

    strncpy(r.inspector, user, sizeof(r.inspector)-1);
    r.inspector[sizeof(r.inspector)-1] = '\0';

    strncpy(r.category, category, sizeof(r.category)-1);
    r.category[sizeof(r.category)-1] = '\0';

    r.severity = severity;

    r.latitude = lat;
    r.longitude = lon;

    r.timestamp = time(NULL);

    strncpy(r.description, desc, sizeof(r.description)-1);
    r.description[sizeof(r.description)-1] = '\0';

    if(write(fd, &r, sizeof(Report)) != sizeof(Report)){
        perror("Write failed");
    }
    close(fd);

    notify_monitor(district, role, user);
    log_action(district, role, user, "ADD REPORT");
}

/* ================= LIST ================= */
void list_reports(char *district, char *role){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    if(!check_permission(path,role,1,0)){
        printf("No read permission!\n");
        return;
    }

    struct stat st;
    if(stat(path, &st) < 0){
        perror("District binary file path not found");
        return;
    }

    char perm[10];
    perm_to_str(st.st_mode,perm);
    printf("Perm:%s Size:%ld Last:%ld\n\n",perm,st.st_size,st.st_mtime);

    int fd=open(path,O_RDONLY);
    if(fd < 0) return;

    Report r;
    while(read(fd,&r,sizeof(Report))>0){
        printf("ID: %d | Inspector: %s | Category: %s | Severity: %d\n",r.id,r.inspector,r.category,r.severity);
    }

    close(fd);
}



/*================== REMOVE DISTRICT (Phase 2)=================*/
void remove_district(char*district, char*role){
    if(strcmp(role,"manager")!=0){
        printf("Access Denied: Only the manager can delete\n");
        return;
    }

    pid_t pid = fork();

    if(pid == 0){
        execlp("rm","rm","-rf",district,NULL);
        perror("Exec rm failed");
        exit(1);
    } else {
        wait(NULL);
    }

    char link[MAX];
    sprintf(link,"active_reports-%s",district);
    unlink(link);
    printf("District '%s' and related active tracking links removed successfully.\n", district);
}

/*=============NOTIFY MONITOR (Phase 2)===============*/
void notify_monitor(char *district, char *role, char *user){
    int fd = open(".monitor_pid", O_RDONLY);
    char logmsg[256];

    if(fd < 0){
        sprintf(logmsg,"Monitor NOT notified (no PID file)\n");
        log_action(district,role,user,logmsg);
        return;
    }

    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf)-1);
    close(fd);

    if(n <= 0){
        sprintf(logmsg,"Monitor NOT notified (read failed)\n");
        log_action(district,role,user,logmsg);
        return;
    }

    pid_t pid = atoi(buf);
    if(kill(pid, SIGUSR1) == -1){
        sprintf(logmsg,"Monitor NOT notified (kill failed)\n");
    } else {
        sprintf(logmsg,"Monitor notified via SIGUSR1 successfully\n");
    }
    log_action(district,role,user,logmsg);
}

/* ================= VIEW REPORT================= */
void view_report(char *district,char*role,int id){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);
    if(!check_permission(path, role, 1, 0)){
        printf("No permission for log!\n");
        return;
    }

    int fd=open(path,O_RDONLY);
    if(fd < 0) return;


    Report r;
    int found = 0;
    while(read(fd, &r, sizeof(Report)) > 0){
        if(r.id == id){
            printf("ID:%d\n Inspector:%s\n Category:%s\n Latitude:%f \nLongitude:%f\n  Severity:%d\n Desc:%s\n ",
                   r.id, r.inspector, r.category, r.latitude,  r.longitude,r.severity, r.description);
            found = 1;
            break;
        }
    }
    if(!found) printf("Report not found\n");
    close(fd);
}

/* ================= REMOVE REPORT================= */
void remove_report(char *district,char*role,char*user,int id){
    if(strcmp(role, "manager") != 0){
        printf("Access Denied: Only manager can remove reports!\n");
        return;
    }
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    int fd=open(path,O_RDWR);
    if(fd < 0) return;

    Report r;
    off_t pos = 0;
    int found = 0;
    while(read(fd,&r,sizeof(Report))>0){
        if(r.id==id){
            found = 1;
            pos = lseek(fd, 0, SEEK_CUR) - sizeof(Report);
            break;
        }
    }
    if(found){
    Report temp;
        // Shift all subsequent records left 
    while(read(fd, &temp, sizeof(Report)) > 0){
        off_t current_read_pos = lseek(fd, 0, SEEK_CUR);
        lseek(fd, pos, SEEK_SET);
        write(fd, &temp, sizeof(temp));
        pos += sizeof(Report);
        lseek(fd, current_read_pos, SEEK_SET);
        }
    struct stat st;
        fstat(fd, &st);
        ftruncate(fd, st.st_size - sizeof(Report));
        printf("Report %d successfully removed via record shift truncation.\n", id);
        log_action(district,role,user,"REMOVE REPORT");
    } else {
        printf("Report not found.\n");
    }

    close(fd);
}

/* ================= UPDATE THRESHOLD================= */
void update_threshold(char *district,char*user,int val,char *role){
    if(strcmp(role,"manager")!=0){
        printf("Access Denied: Only manager can update threshold!\n");
        return;
    }

    char path[MAX];
    sprintf(path,"%s/district.cfg",district);

    struct stat st;
    if(stat(path, &st) < 0) return;


    int fd=open(path,O_WRONLY|O_TRUNC);
    if(fd < 0) return;

    char buf[50];
    sprintf(buf,"threshold=%d\n",val);
    write(fd,buf,strlen(buf));
    close(fd);

    log_action(district,role,user,"UPDATE THRESHOLD");
}

/* ================= FILTER CONDITIONS================= */
int parse_condition(const char *input, char *field, char *op, char *value){
    char temp[256];
    strncpy(temp, input, sizeof(temp)-1);
    temp[sizeof(temp)-1] = '\0';

    char *p1 = strtok(temp, ":");
    char *p2 = strtok(NULL, ":");
    char *p3 = strtok(NULL, "");

    if(!p1 || !p2 || !p3)
        return 0;

    strcpy(field, p1);
    strcpy(op, p2);
    strcpy(value, p3);

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
        int v=atoll(value);
        if(strcmp(op,">=")==0) return r->timestamp>=v;
        if(strcmp(op,"<=")==0) return r->timestamp<=v;
        if(strcmp(op,">")==0) return r->timestamp>v;
        if(strcmp(op,"<")==0) return r->timestamp<v;
        if(strcmp(op,"==")==0) return r->timestamp==v;
        if(strcmp(op,"!=")==0) return r->timestamp!=v;
}

    return 0;
}

void filter_reports(char *district,char*role,int cond_start_idx,int argc,char *argv[]){
    char path[MAX];
    sprintf(path,"%s/reports.dat",district);

    if(!check_permission(path, role, 1, 0)){
        printf("Access Denied: Role '%s' cannot read data paths.\n", role);
        return;
    }

    int fd=open(path,O_RDONLY);
    if( fd < 0) return;

    Report r;

    while(read(fd,&r,sizeof(Report)) > 0){
        int matches_all = 1;

        for(int i=cond_start_idx;i<argc;i++){
            char f[50],o[10],v[50];
            if(!parse_condition(argv[i],f,o,v)){
                continue;
            }

            if(!match_condition(&r,f,o,v)){
                matches_all=0;
                break;
            }
        }

        if(matches_all){
            printf("ID: %d | Inspector: %s | Category: %s | Severity: %d\n",r.id,r.inspector,r.category,r.severity);
        }
    }

    close(fd);
}

/* ================= SCAN DANGLING SYMLINKS ================= */


void check_symlinks(){
    DIR *dir = opendir(".");
    if(!dir) return;

    struct dirent *entry;

    while((entry = readdir(dir)) != NULL){
        struct stat st;

        if(lstat(entry->d_name, &st) < 0) continue;

        if(S_ISLNK(st.st_mode)){
            char target[MAX];
            int len = readlink(entry->d_name, target, sizeof(target)-1);

            if(len >= 0){
                target[len] = '\0';

                struct stat tmp;
                if(stat(target, &tmp) < 0){
                    printf("Warning: Dangling symlink detected: %s\n", entry->d_name);
                }
            }
        }
    }
    closedir(dir);
}


/* ================= MAIN ================= */
int main(int argc,char *argv[]){
    char role_buffer[MAX] = "inspector";
    char user_buffer[MAX] = "default_user";
    char *role = role_buffer;
    char *user = user_buffer;
    
    char *cmd = NULL;
    char *district = NULL;
    int id = 0, val = 0;
    
    // Core allocated string buffers for report items
    char category_buffer[64] = {0};
    char desc_buffer[128] = {0};
    char *category = category_buffer;
    char *desc = desc_buffer;
    
    int severity = 0;
    float lat = 0.0, lon = 0.0;
    int filter_idx = 0;

    //Run dunamic structural cleanup verification hooks natively
    check_symlinks();

    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--role")==0) role=argv[++i];
        else if(strcmp(argv[i],"--user")==0) user=argv[++i];
        else if(strcmp(argv[i],"--add")==0){ cmd="add";
            if (i + 1 < argc) {
                district = argv[++i]; // Safe to advance now
            } else {
                printf("Error: Missing district destination argument after '--add'\n");
                return 1; // Exit cleanly instead of crashing
            }
           
               printf("\n--- Enter New Infrastructure Report Details (%s) ---\n", district);
                printf("-----------------------------------------------------------------\n");
                
                printf("Enter Problem Category (e.g., road, lighting, flooding): ");
                // %63s limits reading to avoid running out of allocated buffer space
                scanf("%63s", category);
                
                printf("Enter Severity Level (1-5): ");
                scanf("%d", &severity);
                
                printf("Enter Coordinates - Latitude: ");
                scanf("%f", &lat);
                
                printf("Enter Coordinates - Longitude: ");
                scanf("%f", &lon);
                
                // CRITICAL: Clean out any remaining newline characters (\n) left in stdin buffer
                while (getchar() != '\n'); 
                
                printf("Enter a brief Description: ");
                // fgets captures spaces seamlessly so descriptions can have multiple words
                fgets(desc, 127, stdin);
                desc[strcspn(desc, "\n")] = 0;
                printf("-----------------------------------------------------------------\n\n");

                while(i + 1 < argc && argv[i + 1][0] != '-') {
                i++;
            }
            
        }
        else if(strcmp(argv[i],"--list")==0){ cmd="list"; district=argv[++i]; }
        else if(strcmp(argv[i],"--view")==0){ cmd="view"; district=argv[++i]; id=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--remove_report")==0){ cmd="remove"; district=argv[++i]; id=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--update_threshold")==0){ cmd="update"; district=argv[++i]; val=atoi(argv[++i]); }
        else if(strcmp(argv[i],"--remove_district")==0){cmd="rm"; district=argv[++i];}
        else if(strcmp(argv[i],"--filter")==0){ cmd="filter"; district=argv[++i]; filter_idx=i+1; break; }
    }
    if(!cmd){
        printf("Missing dynamic system parameters execution options.\n");
        return 1;
    }
    if(!role){ role = "inspector"; } // Default assignment fallback rule
    if(!user){ user = "default_user"; }
    
    if(strcmp(cmd, "add") == 0) {
        struct stat st;
        if(stat(district, &st) < 0) {
            create_district(district); // Ensure this function handles the initial dir creation
        }
    // Only call add_report; let it handle notification and logging internally
        add_report(district, user, role, category, severity, lat, lon, desc);
    }
    else if(strcmp(cmd,"list")==0){
        list_reports(district,role);
    }
    else if(strcmp(cmd,"view")==0){
        view_report(district,role,id);
    }
    else if(strcmp(cmd,"remove")==0){
        remove_report(district,role,user,id);
    }
    else if(strcmp(cmd,"update")==0){
        update_threshold(district,user,val,role);
    }
    else if(strcmp(cmd,"filter")==0){
        filter_reports(district,role,filter_idx,argc,argv);
    }
    else if(strcmp(cmd,"rm")==0){
        remove_district(district,role);
    }
    return 0;
}