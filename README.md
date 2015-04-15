# tvirtual
Project 4: Recoverable Virtual Memory

How you use logfiles to accomplish persistency plus transaction semantics. See Persistency.
What goes in the logfiles? See End of document. 
How do the files get cleaned up? by software call of truncate. truncate is called for every time a commit make a log file become bigger than 2 Mb.

Overview: 
  A memory segment can be retrieved from disk by calling rvm_map()
  Changes are committed to disk by calling rvm_commit_trans()
  Rvm_abort_trans restores a memory segment to its last committed value from the undo log stored in memory.
  Rvm_about_to_modify is required to any modification to be commited
  Additionally truncate updates the backing file and deletes the log files. We give more details next.

Persistency:

Our scheme saves a logfile and backing file per memory segment. Initially the logfiles are empty, on commit we append a transaction record to the files, representing the intent to modify. On truncate, we read the logs, apply it to the backing file, and then delete the logs.

Additionally, we keep a persistent list of the committed transaction in an auxiliar log file. This list is updated at the end of the commit function. This is key to guarantee persistency when writing to multiple segments in the same transaction.

- On map, we retrieve the segment from the backing file; if a log exists, we apply all committed transactions to memory. Therefore, in the worst case, if a crash happened while committing, the system still works flawlessly by simply ignoring uncommited transactions. We also check for the contraints described in the assigment.

- On commit, we write changes specified in the transaction to the log files, after all the segments have been touched, we write the transaction id to the list of committed transactions. (If a file is bigger than x, we call truncate after commit)

- On truncate, if a log exists, we write all committed transactions to the backing file. Next we delete the log file. We repeat this for every segment.

- About_to_modify is required before any modification, thus any commit. When called, we store the undo logs in memory.

- Begin_trans makes sure everything works by not allowing two transactions to modify the same segments. We also have to make sure transaction ids are unique we do that by g


 
