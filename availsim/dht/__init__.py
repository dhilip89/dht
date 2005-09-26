import chord, dhash, oracle, totalrecall

# For automatic use by availsim
known_types = {
    'chord': chord,
    'dhash': dhash.dhash,
    'fragments': dhash.dhash_fragments,
    'replica': dhash.dhash_replica,
    'replica_random': totalrecall.dhash_replica,
    'replica_random_oracle': totalrecall.dhash_replica_oracle,
    'cates': dhash.dhash_cates,
    'durability_oracle_replica': oracle.durability_oracle,
    'availability_oracle_replica': oracle.availability_oracle,
    'total_recall_replica': totalrecall.totalrecall_lazy_replica,
    'total_recall_succplace_replica': dhash.total_recall_replica
}
