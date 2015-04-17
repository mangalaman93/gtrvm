# gtrvm
Project 4: Recoverable Virtual Memory (RVM)

Question: How you use logfiles to accomplish persistency plus transaction semantics? <br/>
Ans: See [Persistency](#persistency) <br/>
<br/>
Question: What goes in the logfiles? <br/>
Ans: See at end of document <br/>
<br/>
Question: How do the files get cleaned up? <br/>
Ans: By software call of truncate, truncate is called by the user of rvm explicitly <br/>

# Implementation
## Overview
 A memory segment can be created or retrieved from disk by calling `rvm_map`. Changes are committed to disk by calling `rvm_commit_trans`. A call to `rvm_abort_trans` restores a memory segment to its last committed value from the undo log stored in memory. `rvm_about_to_modify` is required to be called before making any modifications in the segment. Additionally, a call to truncate, updates the backing file and deletes the log files.

## Persistency
Our scheme saves a log file and backing file per memory segment. Initially the log files are empty, on commit we append a transaction record to the files, representing the intent to modify. On truncate, we read the logs, apply it to the backing file, and then delete the logs.

In summary, before committing, no entries exist in the log file, therefore none of the changes made to memory will persist. After committing log files exist, thus changes will persist. The worst case when a crash happens while commiting. In that case we ignore uncommitted changes when finding that an incomplete entry is present on log files.

## Function details
* On map, we retrieve the segment from the backing file; if a log exists, we apply all committed transactions to memory. In the worst case, if a crash happened while committing, the system still works flawlessly by simply ignoring uncommited transactions. We also check for the constraints described in the assigment.
* On commit, we write changes specified in the transaction to the log files.
* On truncate, if a log exists, we write all committed transactions to the backing file. Next, we delete the log file. We repeat this for every segment.
* About_to_modify is required before any modifications are made to memory segment. When called, we store the undo logs in memory.
* Begin_trans makes sure everything works by not allowing two transactions to modify the same segments. We also have to make sure transaction ids are unique. We check if there are any aborted/crashed transaction by looking for non-duplicates in the committed transaction log.

# Additional test cases.
* `about.c`: checks if multiple calls to about to modify are allowed and if they change the behavior of the program.
* `bigbasic.c`: test if a big string can be written to the memory
* `resize.c`: checks if a segment is increased when mappin a new value
* `truncate2.c`: checks if  memory is consistent after truncate

# Files:
* Backing file: exact copy of the memory to file &lt;segname&gt;.bak
* Log Files: Name: &lt;segname&gt;.log <br/>
  ```Structure: transaction_id (4 B) | region1 | region2 | ... regionN
                region: offset (4 B)| size (4 B) | data (size)
    Example: 1 0 10 HELLOWORLD 11 6 WORLDX <END> 2 0 11 OVERWRITING 3 3 Boo <END>```
* Index: rvm.index : `[<transaction_id>]` <br/>
     Example: 1 1 2 2 3 4 4 3 5 6 6
     In this example transaction 5 was started but wasn't completed
