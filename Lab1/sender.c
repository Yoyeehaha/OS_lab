#include "sender.h"

void send(message_t message, mailbox_t* mailbox_ptr){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, send the message
    */
    if(mailbox_ptr -> flag == 1) mq_send(mailbox_ptr -> storage.msqid, message.message, strlen(message.message) + 1, 0);  // if flag is 1, use message queue to send msg
    if(mailbox_ptr -> flag == 2) strcpy(mailbox_ptr -> storage.shm_addr, message.message);   //if flag is 2, use shared memory
      
}

int main(int argc, char* argv[]){
    /*  TODO: 
        1) Call send(message, &mailbox) according to the flow in slide 4
        2) Measure the total sending time
        3) Get the mechanism and the input file from command line arguments
            • e.g. ./sender 1 input.txt
                    (1 for Message Passing, 2 for Shared Memory)
        4) Get the messages to be sent from the input file
        5) Print information on the console according to the output format
        6) If the message form the input file is EOF, send an exit message to the receiver.c
        7) Print the total sending time and terminate the sender.c
    */
    
    mq_unlink("/msg_queue");     // unlink the message queue
    shm_unlink("/share_memory"); // unlink the shared memory
    
    sem_unlink("/recv_sem");     // unlink the semaphore
    sem_unlink("/send_sem");     // unlink the semaphore

    FILE *file;
    file = fopen(argv[2], "r");   //open and read file
    mailbox_t mailbox;
    mailbox.flag = mailbox.flag = (argv[1][0] == '1') ? 1 : 2;  // get the mechanism from command line arguments
    int shared_mem_fd;                       // shared memory file descriptor
    if(mailbox.flag == 1){
        printf("Message Passing\n");
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = MAX_MESSAGE_SIZE;
        attr.mq_curmsgs = 0;
        mailbox.storage.msqid = mq_open("/msg_queue", O_CREAT | O_RDWR, 0666, &attr); // create the message queue
    }
    
    if(mailbox.flag == 2){
        
        shared_mem_fd = shm_open("/share_memory", O_CREAT | O_RDWR, 0666);   // create the shared memory
        ftruncate(shared_mem_fd, sizeof(char) * MAX_MESSAGE_SIZE);             // truncate the shared memory
        mailbox.storage.shm_addr = mmap(0, sizeof(char) * MAX_MESSAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);  // map the shared memory
        printf("Shared Memory\n");
    }

    sem_t *recv_sem = sem_open("/recv_sem", O_CREAT, 0666, 0);  //create semaphore
    sem_t *send_sem = sem_open("/send_sem", O_CREAT, 0666, 0);

    if(file == NULL){
        perror("File not found\n");
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    double time = 0;

    char buffer[MAX_MESSAGE_SIZE];
    while(fgets(buffer, MAX_MESSAGE_SIZE, file)){   //get msg from input.txt
        message_t send_msg;
        buffer[strcspn(buffer, "\n")] = '\0';
	printf("Sending message: %s\n", buffer);
	strcpy(send_msg.message, buffer);
        clock_gettime(CLOCK_MONOTONIC, &start);
        send(send_msg, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        sem_post(send_sem);   //post sender semaphore
        sem_wait(recv_sem);   //wait receiver semaphore
    }

    if(feof(file)){
        printf("End of input file! Exit!\n");
	message_t over_msg;
        strcpy(over_msg.message, "exit");    //send finish msg to receiver
        clock_gettime(CLOCK_MONOTONIC, &start);
        send(over_msg, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        sem_post(send_sem); // post the semaphore
        sem_wait(recv_sem); // wait the semaphore
    }

    printf("Total taken time in sending message: %f s\n", time);

    sem_close(send_sem); // close the semaphore
    sem_close(recv_sem); // close the semaphore
    if(mailbox.flag == 1){ 
        mq_close(mailbox.storage.msqid);// close the message queue
        mq_unlink("/msq_queue");         // unlink the message queue
    }
    
    if(mailbox.flag == 2){
        munmap(mailbox.storage.shm_addr, sizeof(char) * MAX_MESSAGE_SIZE);// unmap the shared memory
        shm_unlink("/share_memory");                                       // unlink the shared memory
    
    }
    return 0;
}
