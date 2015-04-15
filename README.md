# gtrvm
Project 4: Recoverable Virtual Memory

How you use logfiles to accomplish persistency plus transaction semantics. See Persistency.

What goes in the logfiles? See End of document.

How do the files get cleaned up? by software call of truncate. truncate is called for every time a commit make a log file become bigger than 2 Mb.

# Implementation
## Overview
  A memory segment can be retrieved from disk by calling `rvm_map`. Changes are committed to disk by calling `rvm_commit_trans`. `rvm_abort_trans` restores a memory segment to its last committed value from the undo log stored in memory. `rvm_about_to_modify` is required to any modification to be commited. Additionally truncate updates the backing file and deletes the log files. We give more details next.

## Persistency
Our scheme saves a log file and backing file per memory segment. Initially the logfiles are empty, on commit we append a transaction record to the files, representing the intent to modify. On truncate, we read the logs, apply it to the backing file, and then delete the logs.

Additionally, we keep a persistent list of the committed transaction in an auxiliar log file. This list is updated at the beggining and end of the commit function, in a way that a commited transaction will have two entries and an uncommited will have only one. This is key to guarantee persistency when writing to multiple segments in the same transaction.

In summary, before committing, no entry exists in log file, therefore none of the changes made to memory will persist. After commiting log files exist, thus changes will persist. The worst case is if a crash happens while commiting. In that case we ignore uncommitted changes by not finding them in the index of committed transactions.

## Function details
* On map, we retrieve the segment from the backing file; if a log exists, we apply all committed transactions to memory. Therefore, in the worst case, if a crash happened while committing, the system still works flawlessly by simply ignoring uncommited transactions. We also check for the contraints described in the assigment.
* On commit, we write changes specified in the transaction to the log files, after all the segments have been touched, we write the transaction id to the list of committed transactions. (If a file is bigger than x, we call truncate after commit)
* On truncate, if a log exists, we write all committed transactions to the backing file. Next we delete the log file. We repeat this for every segment.
* About_to_modify is required before any modification, thus any commit. When called, we store the undo logs in memory.
* Begin_trans makes sure everything works by not allowing two transactions to modify the same segments. We also have to make sure transaction ids are unique. We check if there are any aborted/crashed transaction by looking for non-duplicates in the committed transaction log.

# Additional test cases.
* `incomplete.c`: forgets to commit and checks if values are still unchanged
* `crash.c`: interrupts program while commiting to a single segment, checks if program is in a consistent state
* `multi-crash.c`: Interrupts program while commiting to multiple segments, checks if program is in a consistent state

## crash and multi-crash
At crash we interrupt our program while flushing a very long segment to disk. Before committing, no log file exists, therefore none of the changes made to memory will persist. After commiting log files exist, thus changes will persist. The only case is if a crash happens while commiting. So we simulate that. A multi crash writes several segments in a single transaction, and we interrupt the program while doing that. Depending on the way the program was designed it's easy to forget to guarantee consisteny between multiple segments.

# Files:
* Backing file: exact copy of the memory to file <segname>.bak
* Log Files: Name: <segname>.log </br>
  ```Structure: transaction_id (4 B) | region1 | region2 | ... regionN
                region: offset (4 B)| size (4 B) | data (size)
    Example: 1 0 10 HELLOWORLD 11 6 WORLDX <END> 2 0 11 OVERWRITING 3 3 Boo <END>```
* Index: rvm.index </br> : [<transaction_id>]
     Example: 1 1 2 2 3 4 4 3 5 6 6
     In this example transaction 5 was started but wasn't completed
