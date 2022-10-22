
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



//--------------- structures declaration of train and user------------//
struct train{
		int train_number;
		char train_name[50];
		int total_seats;
		int available_seats;
		};
struct user{
		int login_id;
		char password[50];
		char name[50];
		int type;
		};

struct booking{
		int booking_id;
		int type;
		int uid;
		int tid;
		int seats;
		};
//---------------------------------------------------------------------//

void service_client(int sock);
void login(int client_sock);
void signup(int client_sock);
int menu(int client_sock,int type,int id);
void crud_train(int client_sock);
void crud_user(int client_sock);
int user_function(int client_sock,int choice,int type,int id);
   
int main() {
 
    int sd, nsd,c; 
    struct sockaddr_in server, client; 
    char buf[100]; 
  
    sd = socket(AF_INET, SOCK_STREAM, 0); 
     
    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8000); 
   
    bind(sd, (struct sockaddr*)&server, sizeof(server));
        
    listen(sd, 3);  
    c = sizeof(struct sockaddr_in); 
  
    while (1){

	    nsd = accept(sd, (struct sockaddr*)&client, (socklen_t*)&c); 
	  
	    if (!fork()){
		    close(sd);
		    service_client(nsd);								// Service client, once done client exits
		    exit(0);
	    }
	    else
	    	close(nsd);
    }
    return 0;
}

//-------------------- Service every client-----------------------------//
void service_client(int nsd){
	int choice;
	
	do{
		read(nsd, &choice, sizeof(int));		
		if(choice==1)
			login(nsd);
		if(choice==2)
			signup(nsd);
		if(choice==3)
			break;
	}while(1);

	close(nsd);
	
}

//-------------------- Login function-----------------------------//

void login(int cli_sock){
	int fd_user = open("db/db_user",O_RDWR);
	int id,type,valid=0,user_valid=0;
	char password[50];
	struct user u;
	read(cli_sock,&id,sizeof(id));
	read(cli_sock,&password,sizeof(password));
	
	struct flock lock;
	
	lock.l_start = (id-1)*sizeof(struct user);
	lock.l_len = sizeof(struct user);
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();

	lock.l_type = F_WRLCK;
	fcntl(fd_user,F_SETLKW, &lock);
        
	
	while(read(fd_user,&u,sizeof(u))){
		if(u.login_id==id){
			user_valid=1;
			if(!strcmp(u.password,password)){
				valid = 1;
				type = u.type;
				break;
			}
			else{
				valid = 0;
				break;
			}	
		}		
		else{
			user_valid = 0;
			valid=0;
		}
	}
	
	// same agent is allowed from multiple terminals.. 
	// so unlocking his user record just after checking his credentials and allowing further
	if(type!=2){
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}
	
	// if valid user, show him menu
	
        if(user_valid)
	{
		write(cli_sock,&valid,sizeof(valid));
		if(valid){
			write(cli_sock,&type,sizeof(type));
			while(menu(cli_sock,type,id)!=-1);
		}
	}
	else
		write(cli_sock,&valid,sizeof(valid));
	
	// same user is not allowed from multiple terminals.. 
	// so unlocking his user record after he logs out only to not allow him from other terminal
	if(type==2){
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}
} 

//-------------------- Signup function-----------------------------//

void signup(int cli_sock){
	int fd_user = open("db/db_user",O_RDWR);
	int type,id=0;
	char name[50],password[50];
	struct user u,temp;

	read(cli_sock, &type, sizeof(type));
	read(cli_sock, &name, sizeof(name));
	read(cli_sock, &password, sizeof(password));

	int fp = lseek(fd_user, 0, SEEK_END);

	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_start = fp;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();


	fcntl(fd_user,F_SETLKW, &lock);

	// if file is empty, login id will start from 1
	// else it will increment from the previous value
	if(fp==0){
		u.login_id = 1;
		strcpy(u.name, name);
		strcpy(u.password, password);
		u.type=type;
		write(fd_user, &u, sizeof(u));
		write(cli_sock, &u.login_id, sizeof(u.login_id));

		
	}
	else{
		fp = lseek(fd_user, -1 * sizeof(struct user), SEEK_END);
		read(fd_user, &u, sizeof(u));
		u.login_id++;
		strcpy(u.name, name);
		strcpy(u.password, password);
		u.type=type;
		write(fd_user, &u, sizeof(u));
		write(cli_sock, &u.login_id, sizeof(u.login_id));

	}
	lock.l_type = F_UNLCK;
	fcntl(fd_user, F_SETLK, &lock);

	close(fd_user);
	
}

//-------------------- Main menu function-----------------------------//

int menu(int client_sock,int type,int id){

       int choice,ret;

	// for admin
	if(type==0){
		read(client_sock,&choice,sizeof(choice));
		if(choice==1){					// CRUD options on train
			crud_train(client_sock);
			return menu(client_sock,type,id);
		}
		else if(choice==2){				// CRUD options on User
			crud_user(client_sock);
			return menu(client_sock,type,id);
		}
		else if (choice ==3)				// Logout
			return -1;
	}
	else if(type==2 || type==1){				// For agent and customer
		read(client_sock,&choice,sizeof(choice));
		ret = user_function(client_sock,choice,type,id);
		if(ret!=5)
			return menu(client_sock,type,id);
		else if(ret==5)
			return -1;
	}		
}

//--------------------- operation on train--------------------//

void crud_train(int cli_sock){
	int valid=0;	
	int choice;
	read(cli_sock,&choice,sizeof(choice));
	if(choice==1){  					// Add train  	
		char tname[50];
		int tid = 0;
		read(cli_sock,&tname,sizeof(tname));
		struct train t,temp;
		struct flock lock;
		int fd_train = open("db/db_train", O_RDWR);
		
		t.train_number = tid;
		strcpy(t.train_name,tname);
		t.total_seats = 10;				// by default, we are taking 10 seats
		t.available_seats = 10;

		int fp = lseek(fd_train, 0, SEEK_END); 

		lock.l_type = F_WRLCK;
		lock.l_start = fp;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd_train, F_SETLKW, &lock);

		if(fp == 0){
			valid = 1;
			write(fd_train, &t, sizeof(t));
			lock.l_type = F_UNLCK;
			fcntl(fd_train, F_SETLK, &lock);
			close(fd_train);
			write(cli_sock, &valid, sizeof(valid));
		}
		else{
			valid = 1;
			lseek(fd_train, -1 * sizeof(struct train), SEEK_END);
			read(fd_train, &temp, sizeof(temp));
			t.train_number = temp.train_number + 1;
			write(fd_train, &t, sizeof(t));
			write(cli_sock, &valid,sizeof(valid));	
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);
		
	}

	else if(choice==2){					// View/ Read train db
		struct flock lock;
		struct train t;
		int fd_train = open("db/db_train", O_RDONLY);
		
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lock);
		int fp = lseek(fd_train, 0, SEEK_END);
		int no_of_trains = fp / sizeof(struct train);
		write(cli_sock, &no_of_trains, sizeof(int));

		lseek(fd_train,0,SEEK_SET);
		while(no_of_trains--){
			read(fd_train,&t,sizeof(t));
			write(cli_sock,&t.train_number,sizeof(int));
			write(cli_sock,&t.train_name,sizeof(t.train_name));
			write(cli_sock,&t.total_seats,sizeof(int));
			write(cli_sock,&t.available_seats,sizeof(int));
		}
		valid = 1;
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);
	}

	else if(choice==3){					// Update train
		crud_train(cli_sock);
		int choice,valid=0,tid;
		struct flock lock;
		struct train t;
		int fd_train = open("db/db_train", O_RDWR);

		read(cli_sock,&tid,sizeof(tid));

		lock.l_type = F_WRLCK;
		lock.l_start = (tid)*sizeof(struct train);
		lock.l_len = sizeof(struct train);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lock);

		lseek(fd_train, 0, SEEK_SET);
		lseek(fd_train, (tid)*sizeof(struct train), SEEK_CUR);
		read(fd_train, &t, sizeof(struct train));
		
		read(cli_sock,&choice,sizeof(int));
		if(choice==1){							// update train name
			write(cli_sock,&t.train_name,sizeof(t.train_name));
			read(cli_sock,&t.train_name,sizeof(t.train_name));
			
		}
		else if(choice==2){						// update total number of seats
			write(cli_sock,&t.total_seats,sizeof(t.total_seats));
			read(cli_sock,&t.total_seats,sizeof(t.total_seats));
		}
	
		lseek(fd_train, -1*sizeof(struct train), SEEK_CUR);
		write(fd_train, &t, sizeof(struct train));
		valid=1;
		write(cli_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);	
	}

	else if(choice==4){						// Delete train
		crud_train(cli_sock);
		struct flock lock;
		struct train tdb;
		int fd_train = open("db/db_train", O_RDWR);
		int tid,valid=0;

		read(cli_sock,&tid,sizeof(tid));

		lock.l_type = F_WRLCK;
		lock.l_start = (tid)*sizeof(struct train);
		lock.l_len = sizeof(struct train);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lock);
		
		lseek(fd_train, 0, SEEK_SET);
		lseek(fd_train, (tid)*sizeof(struct train), SEEK_CUR);
		read(fd_train, &tdb, sizeof(struct train));
		strcpy(tdb.train_name,"deleted");
		lseek(fd_train, -1*sizeof(struct train), SEEK_CUR);
		write(fd_train, &tdb, sizeof(struct train));
		valid=1;
		write(cli_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);	
	}	
}

//---------------------- operation on user--------------------//
void crud_user(int client_sock){
	int valid=0;	
	int choice;
	read(client_sock,&choice,sizeof(choice));
	if(choice==1){    					// Add user
		char name[50],password[50];
		int type;
		read(client_sock, &type, sizeof(type));
		read(client_sock, &name, sizeof(name));
		read(client_sock, &password, sizeof(password));
		
		struct user udb;
		struct flock lock;
		int fd_user = open("db/db_user", O_RDWR);
		int fp = lseek(fd_user, 0, SEEK_END);
		
		lock.l_type = F_WRLCK;
		lock.l_start = fp;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd_user, F_SETLKW, &lock);

		if(fp==0){
			udb.login_id = 1;
			strcpy(udb.name, name);
			strcpy(udb.password, password);
			udb.type=type;
			write(fd_user, &udb, sizeof(udb));
			valid = 1;
			write(client_sock,&valid,sizeof(int));
			write(client_sock, &udb.login_id, sizeof(udb.login_id));
			
		}
		else{
			fp = lseek(fd_user, -1 * sizeof(struct user), SEEK_END);
			read(fd_user, &udb, sizeof(udb));
			udb.login_id++;
			strcpy(udb.name, name);
			strcpy(udb.password, password);
			udb.type=type;
			write(fd_user, &udb, sizeof(udb));
			valid = 1;
			write(client_sock,&valid,sizeof(int));
			write(client_sock, &udb.login_id, sizeof(udb.login_id));
			
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
		
	}

	else if(choice==2){					// View user list
		struct flock lock;
		struct user udb;
		int fd_user = open("db/db_user", O_RDONLY);
		
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);
		int fp = lseek(fd_user, 0, SEEK_END);
		int no_of_users = fp / sizeof(struct user);
		no_of_users--;
		write(client_sock, &no_of_users, sizeof(int));

		lseek(fd_user,0,SEEK_SET);
		while(no_of_users--){
			read(fd_user,&udb,sizeof(udb));
			if(udb.type!=0){
				write(client_sock,&udb.login_id,sizeof(int));
				write(client_sock,&udb.name,sizeof(udb.name));
				write(client_sock,&udb.type,sizeof(int));
			}
		}
		valid = 1;
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}

	else if(choice==3){					// Update user
		crud_user(client_sock);
		int choice,valid=0,uid;
		char pass[50];
		struct flock lock;
		struct user udb;
		int fd_user = open("db/db_user", O_RDWR);

		read(client_sock,&uid,sizeof(uid));

		lock.l_type = F_WRLCK;
		lock.l_start =  (uid-1)*sizeof(struct user);
		lock.l_len = sizeof(struct user);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);

		lseek(fd_user, 0, SEEK_SET);
		lseek(fd_user, (uid-1)*sizeof(struct user), SEEK_CUR);
		read(fd_user, &udb, sizeof(struct user));
		
		read(client_sock,&choice,sizeof(int));
		if(choice==1){					// update name
			write(client_sock,&udb.name,sizeof(udb.name));
			read(client_sock,&udb.name,sizeof(udb.name));
			valid=1;
			write(client_sock,&valid,sizeof(valid));		
		}
		else if(choice==2){				// update password
			read(client_sock,&pass,sizeof(pass));
			if(!strcmp(udb.password,pass))
				valid = 1;
			write(client_sock,&valid,sizeof(valid));
			read(client_sock,&udb.password,sizeof(udb.password));
		}
	
		lseek(fd_user, -1*sizeof(struct user), SEEK_CUR);
		write(fd_user, &udb, sizeof(struct user));
		if(valid)
			write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);	
	}

	else if(choice==4){						// Delete any particular user
		crud_user(client_sock);
		struct flock lock;
		struct user udb;
		int fd_user = open("db/db_user", O_RDWR);
		int uid,valid=0;

		read(client_sock,&uid,sizeof(uid));

		lock.l_type = F_WRLCK;
		lock.l_start =  (uid-1)*sizeof(struct user);
		lock.l_len = sizeof(struct user);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);
		
		lseek(fd_user, 0, SEEK_SET);
		lseek(fd_user, (uid-1)*sizeof(struct user), SEEK_CUR);
		read(fd_user, &udb, sizeof(struct user));
		strcpy(udb.name,"deleted");
		strcpy(udb.password,"");
		lseek(fd_user, -1*sizeof(struct user), SEEK_CUR);
		write(fd_user, &udb, sizeof(struct user));
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);	
	}
}


//---------------------- User functions -----------------------//
int user_function(int cli_sock,int choice,int type,int id){
	int valid=0;
	if(choice==1){						// book ticket
		crud_train(cli_sock);
		struct flock lockt;
		struct flock lockb;
		struct train t;
		struct booking b;
		int fd_train = open("db/db_train", O_RDWR);
		int fd_book = open("db/db_booking", O_RDWR);
		int tid,seats;
		read(cli_sock,&tid,sizeof(tid));		
				
		lockt.l_type = F_WRLCK;
		lockt.l_start = tid*sizeof(struct train);
		lockt.l_len = sizeof(struct train);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();
		
		lockb.l_type = F_WRLCK;
		lockb.l_start = 0;
		lockb.l_len = 0;
		lockb.l_whence = SEEK_END;
		lockb.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lockt);
		lseek(fd_train,tid*sizeof(struct train),SEEK_SET);
		
		read(fd_train,&t,sizeof(t));
		read(cli_sock,&seats,sizeof(seats));

		if(t.train_number==tid)
		{		
			if(t.available_seats>=seats){
				valid = 1;
				t.available_seats -= seats;
				fcntl(fd_book, F_SETLKW, &lockb);
				int fp = lseek(fd_book, 0, SEEK_END);
				
				if(fp > 0){
					lseek(fd_book, -1*sizeof(struct booking), SEEK_CUR);
					read(fd_book, &b, sizeof(struct booking));
					b.booking_id++;
				}
				else 
					b.booking_id = 0;

				b.type = type;
				b.uid = id;
				b.tid = tid;
				b.seats = seats;
				write(fd_book, &b, sizeof(struct booking));
				lockb.l_type = F_UNLCK;
				fcntl(fd_book, F_SETLK, &lockb);
			 	close(fd_book);
			}
		
		lseek(fd_train, -1*sizeof(struct train), SEEK_CUR);
		write(fd_train, &t, sizeof(t));
		}

		lockt.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lockt);
		close(fd_train);
		write(cli_sock,&valid,sizeof(valid));
		return valid;		
	}
	
	else if(choice==2){							// View bookings
		struct flock lock;
		struct booking b;
		int fd_book = open("db/db_booking", O_RDONLY);
		int no_of_bookings = 0;
	
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lock);
	
		while(read(fd_book,&b,sizeof(b))){
			if (b.uid==id)
				no_of_bookings++;
		}

		write(cli_sock, &no_of_bookings, sizeof(int));
		lseek(fd_book,0,SEEK_SET);

		while(read(fd_book,&b,sizeof(b))){
			if(b.uid==id){
				write(cli_sock,&b.booking_id,sizeof(int));
				write(cli_sock,&b.tid,sizeof(int));
				write(cli_sock,&b.seats,sizeof(int));
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lock);
		close(fd_book);
		return valid;
	}

	else if (choice==3){							// update booking
		int choice = 2,bid,val;
		user_function(cli_sock,choice,type,id);
		struct booking bdb;
		struct train tdb;
		struct flock lockb;
		struct flock lockt;
		int fd_book = open("db/db_booking", O_RDWR);
		int fd_train = open("db/db_train", O_RDWR);
		read(cli_sock,&bid,sizeof(bid));

		lockb.l_type = F_WRLCK;
		lockb.l_start = bid*sizeof(struct booking);
		lockb.l_len = sizeof(struct booking);
		lockb.l_whence = SEEK_SET;
		lockb.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lockb);
		lseek(fd_book,bid*sizeof(struct booking),SEEK_SET);
		read(fd_book,&bdb,sizeof(bdb));
		lseek(fd_book,-1*sizeof(struct booking),SEEK_CUR);
		
		lockt.l_type = F_WRLCK;
		lockt.l_start = (bdb.tid)*sizeof(struct train);
		lockt.l_len = sizeof(struct train);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();

		fcntl(fd_train, F_SETLKW, &lockt);
		lseek(fd_train,(bdb.tid)*sizeof(struct train),SEEK_SET);
		read(fd_train,&tdb,sizeof(tdb));
		lseek(fd_train,-1*sizeof(struct train),SEEK_CUR);

		read(cli_sock,&choice,sizeof(choice));
	
		if(choice==1){							// increase number of seats required of booking id
			read(cli_sock,&val,sizeof(val));
			if(tdb.available_seats>=val){
				valid=1;
				tdb.available_seats -= val;
				bdb.seats += val;
			}
		}
		else if(choice==2){						// decrease number of seats required of booking id
			valid=1;
			read(cli_sock,&val,sizeof(val));
			tdb.available_seats += val;
			bdb.seats -= val;	
		}
		
		write(fd_train,&tdb,sizeof(tdb));
		lockt.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lockt);
		close(fd_train);
		
		write(fd_book,&bdb,sizeof(bdb));
		lockb.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lockb);
		close(fd_book);
		
		write(cli_sock,&valid,sizeof(valid));
		return valid;
	}
	else if(choice==4){							// Cancel an entire booking
		int choice = 2,bid;
		user_function(cli_sock,choice,type,id);
		struct booking bdb;
		struct train tdb;
		struct flock lockb;
		struct flock lockt;
		int fd_book = open("db/db_booking", O_RDWR);
		int fd_train = open("db/db_train", O_RDWR);
		read(cli_sock,&bid,sizeof(bid));

		lockb.l_type = F_WRLCK;
		lockb.l_start = bid*sizeof(struct booking);
		lockb.l_len = sizeof(struct booking);
		lockb.l_whence = SEEK_SET;
		lockb.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lockb);
		lseek(fd_book,bid*sizeof(struct booking),SEEK_SET);
		read(fd_book,&bdb,sizeof(bdb));
		lseek(fd_book,-1*sizeof(struct booking),SEEK_CUR);
		
		lockt.l_type = F_WRLCK;
		lockt.l_start = (bdb.tid)*sizeof(struct train);
		lockt.l_len = sizeof(struct train);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();

		fcntl(fd_train, F_SETLKW, &lockt);
		lseek(fd_train,(bdb.tid)*sizeof(struct train),SEEK_SET);
		read(fd_train,&tdb,sizeof(tdb));
		lseek(fd_train,-1*sizeof(struct train),SEEK_CUR);

		tdb.available_seats += bdb.seats;
		bdb.seats = 0;
		valid = 1;

		write(fd_train,&tdb,sizeof(tdb));
		lockt.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lockt);
		close(fd_train);
		
		write(fd_book,&bdb,sizeof(bdb));
		lockb.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lockb);
		close(fd_book);
		
		write(cli_sock,&valid,sizeof(valid));
		return valid;
		
	}
	else if(choice==5)										// Logout
		return 5;

}

