library("reshape")
library("ggplot2")
library("ggthemes")
library("scales")
library("sitools")
library("Cairo")

INPUT="/Volumes/barracuda/Code/seqan/apps/bench/scripts/resources"
OUTPUT="/Users/esiragusa/Documents/Dissertation/plots"

PDF_SCALE=0.4
PDF_WIDTH=15
PDF_HEIGHT=7
FONT_SIZE=10
POINT_SIZE=2
FONT_FAMILY='Cambria'

FILTER_NAMES=c("seeds_0","seeds_1","seeds_2","qgrams_0","qgrams_1","qgrams_gapped")
FILTER_LABELS=c("Exact seeds",
                "1-Apx seeds",
                "2-Apx seeds",
                "q-Grams, t \u2265 1",
                "q-Grams, t \u2265 4",
                "Gapped q-grams")
FILTER_COLORS <- c("#D62728", "#FF7F0E", "#8C564B", "#9467BD", "#1F77B4", "#2CA02C")
names(FILTER_COLORS) <- FILTER_NAMES
FILTER_SHAPES <- c(0,1,2,3,4,5)
names(FILTER_SHAPES) <- FILTER_NAMES

DATASET='celegans'
ALPHABET='dna'
ALPHSIZE=4
PLENGTH=100
DISTANCES=c('hamming','edit')
ERROR_LABELS=list()
ERROR_LABELS[['hamming']]='Mismatches'
ERROR_LABELS[['edit']]='Differences'

### FUNCTIONS ###

options(scipen=999)

checkNA <- function(y)
{
    any(is.na(y))
}

load_file <- function(filename)
{
    if (file.exists(filename))
    {
        # try the main resource file first
        print(paste("read",filename))
        tsvFile = try(read.delim(filename, header=TRUE))
        if (inherits(tsvFile,"try-error"))
        {
            print(paste("EMPTY FILE:", filename))
            return(list(ok=FALSE))
        }
        if (nrow(tsvFile) < 1)
        {
            print(paste("BAD FORMAT:", filename))
            return(list(ok=FALSE))
        }
        if (any(apply(tsvFile, 1, checkNA)))
        {
            print(paste("BAD FORMAT:", filename))
            return(list(ok=FALSE))
        }
        return(list(ok=TRUE,tsv=tsvFile))
    } else
        return(list(ok=FALSE))
}

# Copy over qgrams_0 rows to qgrams_1 absent rows
copy_qgrams_rows <- function(df)
{
  df_Q1 = subset(df, filter=='qgrams_1')
  df_Q0 = subset(df, filter=='qgrams_0')
  df_Q = rbind(df_Q1, df_Q0)
  df_Q = df_Q[!duplicated(df_Q[, c('alphabet', 'dataset', 'pcount', 'plength', 'errors', 'distance')]), ]
  df_Q0 = subset(df_Q, filter=='qgrams_0')
  df_Q0$filter[df_Q0$filter=='qgrams_0'] <- 'qgrams_1'
  df_Q0$time <- NA
  df_Q0$occurrences <- NA
  df_Q0$duplicates <- NA
  df_Q0$verifications <- NA
  df = rbind(df, df_Q0)
  return(df)
}

### LOAD FILES ###

FILENAME_OCCS=paste(paste(INPUT, 'filter_occurrences', sep='/'), "tsv", sep='.')
FILENAME_VERIFY=paste(paste(INPUT, 'filter_verify', sep='/'), "tsv", sep='.')
FILENAME_FILTER=paste(paste(INPUT, 'filter_only', sep='/'), "tsv", sep='.')
  
if ((R = load_file(FILENAME_OCCS))$ok)
{
  TABLE_OCCS <- R$tsv;
} else
  print(paste("NOT FOUND:", FILENAME_OCCS))

if ((R = load_file(FILENAME_VERIFY))$ok)
{
  TABLE_VERIFY <- R$tsv;
} else
  print(paste("NOT FOUND:", FILENAME_VERIFY))

if ((R = load_file(FILENAME_FILTER))$ok)
{
  TABLE_FILTER <- R$tsv;
} else
  print(paste("NOT FOUND:", FILENAME_FILTER))

TABLE_OCCS = subset(TABLE_OCCS, select=c(alphabet, dataset, pcount, plength, errors, distance, occurrences))

TABLE_VERIFY = copy_qgrams_rows(TABLE_VERIFY)
TABLE_FILTER = copy_qgrams_rows(TABLE_FILTER)
TABLE_VERIFY = subset(TABLE_VERIFY, select=-occurrences)
TABLE_FILTER = subset(TABLE_FILTER, select=-occurrences)
TABLE_FILTER = subset(TABLE_FILTER, select=-duplicates)

TABLE_FULL = merge(TABLE_VERIFY, TABLE_OCCS, by=c('alphabet', 'dataset', 'pcount', 'plength', 'errors', 'distance'), all.x=TRUE)

TABLE_RUNTIME = merge(TABLE_FILTER, TABLE_VERIFY, by=c('alphabet', 'dataset', 'pcount', 'plength', 'errors', 'distance', 'filter'), all=TRUE)
TABLE_RUNTIME = rename(TABLE_RUNTIME, c("time.x"="ftime", "time.y"="time"))
TABLE_RUNTIME = transform(TABLE_RUNTIME, vtime = time - ftime)
TABLE_RUNTIME$vtime[TABLE_RUNTIME$vtime < 0.01] <- 0.01


### PLOT PPV ###

for (DISTANCE in DISTANCES)
{
  PLOT_PPV = paste(paste(OUTPUT, "filter_ppv", sep='/'), ALPHABET, DATASET, DISTANCE, PLENGTH, "pdf", sep='.')
  
  table_ppv = subset(TABLE_FULL, alphabet==ALPHABET & dataset==DATASET & plength==PLENGTH & distance==DISTANCE, select=c(filter, errors, verifications, duplicates, occurrences))
  table_ppv$ppv <- table_ppv$occurrences / table_ppv$verification
  
  ggplot() +
    geom_line(data=table_ppv, aes(x=errors, y=ppv, group=filter, shape=filter, color=filter), linetype='solid') +
    geom_point(data=table_ppv, aes(x=errors, y=ppv, group=filter, shape=filter, color=filter), size=POINT_SIZE) +
    scale_shape_manual(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS, values=FILTER_SHAPES) +
    scale_color_manual(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS, values=FILTER_COLORS) +
    xlab(ERROR_LABELS[[DISTANCE]]) +
    ylab("PPV [matches/verifications]") +
    scale_x_discrete(breaks=c(2,4,6,8,10)) +
    scale_y_log10() +
    theme_bw(base_size=FONT_SIZE, base_family=FONT_FAMILY)
  
  ggsave(file=PLOT_PPV, width=PDF_WIDTH, height=PDF_HEIGHT, scale=PDF_SCALE, device=cairo_pdf) 
}


### PLOT FILTRATION RATIO ###

for (DISTANCE in DISTANCES)
{
  table_ratio = subset(TABLE_RUNTIME, alphabet==ALPHABET & dataset==DATASET & distance==DISTANCE & plength==PLENGTH, select=c(filter, errors, pcount, ftime, vtime))
  table_ratio <- transform(table_ratio, ratio = vtime/ftime)
  
  PLOT_RATIO=paste(paste(OUTPUT, "filter_ratio", sep='/'), ALPHABET, DATASET, DISTANCE, PLENGTH, "pdf", sep='.')
  
  ggplot() +
    geom_line(data=table_ratio, aes(x=errors, y=ratio, group=filter, shape=filter, color=filter), linetype='solid') +
    geom_point(data=table_ratio, aes(x=errors, y=ratio, group=filter, shape=filter, color=filter), size=POINT_SIZE) +
    scale_shape_manual(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS, values=FILTER_SHAPES) +
    scale_color_manual(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS, values=FILTER_COLORS) +
    xlab(ERROR_LABELS[[DISTANCE]]) +
    ylab("Verification/filtration time [s]") +
    scale_x_discrete(breaks=c(2,4,6,8,10)) +
    scale_y_log10(labels=f2si) +
    theme_bw(base_size=FONT_SIZE, base_family=FONT_FAMILY)
  
  ggsave(file=PLOT_RATIO, width=PDF_WIDTH, height=PDF_HEIGHT, scale=PDF_SCALE, device=cairo_pdf) 
}


### PLOT RUNTIME ###

for (DISTANCE in DISTANCES)
{
  table_runtime = subset(TABLE_RUNTIME, alphabet==ALPHABET & dataset==DATASET & distance==DISTANCE & plength==PLENGTH, select=c(filter, errors, pcount, time, ftime))
  table_runtime <- transform(table_runtime, time = time / pcount, ftime = ftime / pcount)
  
  PLOT_RUNTIME=paste(paste(OUTPUT, "filter_runtime", sep='/'), ALPHABET, DATASET, DISTANCE, PLENGTH, "pdf", sep='.')
  
  ggplot() +
    geom_line(data=table_runtime, aes(x=errors, y=time, group=filter, shape=filter, color=filter), linetype='solid') +
    geom_point(data=table_runtime, aes(x=errors, y=time, group=filter, shape=filter, color=filter), size=POINT_SIZE) +
    geom_line(data=table_runtime, aes(x=errors, y=ftime, group=filter, shape=filter, color=filter), linetype='dashed') +
    geom_point(data=table_runtime, aes(x=errors, y=ftime, group=filter, shape=filter, color=filter), size=POINT_SIZE-1) +
    scale_shape_manual(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS, values=FILTER_SHAPES) +
    scale_color_manual(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS, values=FILTER_COLORS) +
    xlab(ERROR_LABELS[[DISTANCE]]) +
    ylab("Average time per pattern [s]") +
    scale_x_discrete(breaks=c(2,4,6,8,10)) +
    scale_y_log10(labels=f2si) +
    theme_bw(base_size=FONT_SIZE, base_family=FONT_FAMILY)
  
  ggsave(file=PLOT_RUNTIME, width=PDF_WIDTH, height=PDF_HEIGHT, scale=PDF_SCALE, device=cairo_pdf) 
}


### PLOT ONLINE VS OFFLINE RUNTIME ###

# ERRORS=4
# 
# FILENAME_ONOFF=paste(paste(INPUT, 'onoff', sep='/'), "tsv", sep='.')
# PLOT_ONOFF=paste(paste(OUTPUT, "onoff", sep='/'), ALPHABET, DATASET, DISTANCE, PLENGTH, "pdf", sep='.')
# 
# if ((R = load_file(FILENAME_ONOFF))$ok)
# {
#   TABLE_ONOFF <- R$tsv;
# } else
#   print(paste("NOT FOUND:", FILENAME_ONOFF))
# 
# table_onoff = subset(TABLE_ONOFF, alphabet==ALPHABET & dataset==DATASET & plength==PLENGTH & errors==ERRORS, select=c(filter, pcount, time))
# table_onoff <- transform(table_onoff, time = time / pcount)
# 
# ggplot() +
#   geom_line(data=table_onoff, aes(x=pcount, y=time, group=filter, shape=filter, color=filter), linetype='solid') +
#   geom_point(data=table_onoff, aes(x=pcount, y=time, group=filter, shape=filter, color=filter), size=POINT_SIZE) +
#   scale_shape_discrete(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS) +
#   scale_color_discrete(name="Filter", breaks=FILTER_NAMES, labels=FILTER_LABELS) +
#   xlab("Patterns") +
#   ylab("Time (seconds/patterns)") +
#   scale_x_log10() +
#   theme_bw(base_size=FONT_SIZE, base_family=FONT_FAMILY)
# 
# ggsave(file=PLOT_ONOFF, scale=PDF_SCALE, device=cairo_pdf) 


### PLOT VERIFICATIONS ###

# TABLE_VERIFICATIONS <- data.frame()
# for (FILTER in FILTER_NAMES)
# {
#   FILENAME_FILTER=paste(paste(INPUT, FILTER, sep='/'), "tsv", sep='.')
#   
#   if ((R = load_file(FILENAME_FILTER))$ok)
#   {
#     TABLE_VERIFICATIONS <- R$tsv;
#   } else
#     print(paste("NOT FOUND:", FILENAME_FILTER))
# }

