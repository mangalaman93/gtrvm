# tvirtual
Project 4: Recoverable Virtual Memory

On Init we setupt the directory.

On map we retrieve the segment from the backing file, then we check if a log exist for that segment, if a log is existent we apply the committed changes.

On commit we write changes specified in the transaction to a log file.

On truncate we update the backing file with the log file information, then delete the log file.
