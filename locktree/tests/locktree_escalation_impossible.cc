/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

namespace toku {

static uint64_t get_status(LTM_STATUS ltm_status, const char *keyname) {
    TOKU_ENGINE_STATUS_ROW key_status = NULL;
    // lookup keyname in status
    for (int i = 0; ; i++) {
        TOKU_ENGINE_STATUS_ROW status = &ltm_status->status[i];
        if (status->keyname == NULL)
            break;
        if (strcmp(status->keyname, keyname) == 0) {
            key_status = status;
            break;
        }
    }
    assert(key_status);
    return key_status->value.num;
}

// verify that a sequence of point locks owned by alternating transactions can not be escalated
void locktree_unit_test::test_escalation(void) {
    int r;

    // create a manager
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);

    r = mgr.set_max_lock_memory(1000000);
    assert(r == 0);

    LTM_STATUS_S status;
    mgr.get_status(&status);
    assert(get_status(&status, "LTM_ESCALATION_COUNT") == 0);

    // create a locktree
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);
    assert(lt);

    const TXNID txnid_a = 1000;
    const TXNID txnid_b = 2001;

    // txnid_a locks the even numbers
    // txnid_b locks the odd numbers
    // eventually escalation runs, fails, and we are out of memory
    
    const int64_t max_i = 100000;
    for (int64_t i = 0; i < max_i; i++) {
        r = locktree_write_lock(lt, txnid_a, 2*i, 2*i);
        if (r != 0) {
            assert(r == TOKUDB_OUT_OF_LOCKS);
            break;
        }
        r = locktree_write_lock(lt, txnid_b, 2*i+1, 2*i+1);
        if (r != 0) {
            assert(r == TOKUDB_OUT_OF_LOCKS);
            break;
        }
    }

    // ensure that escalation ran
    mgr.get_status(&status);
    assert(get_status(&status, "LTM_ESCALATION_COUNT") == 1);

    // release locks
    for (int64_t i = 0; i < max_i; i++) {
        DBT left; int64_t left_k = 2*i; toku_fill_dbt(&left, &left_k, sizeof left_k);
        DBT right; int64_t right_k = 2*i; toku_fill_dbt(&right, &right_k, sizeof right_k);
        locktree_test_release_lock(lt, txnid_a, &left, &right);
    }
    for (int64_t i = 0; i < max_i; i++) {
        DBT left; int64_t left_k = 2*i+1; toku_fill_dbt(&left, &left_k, sizeof left_k);
        DBT right; int64_t right_k = 2*i+1; toku_fill_dbt(&right, &right_k, sizeof right_k);
        locktree_test_release_lock(lt, txnid_b, &left, &right);
    }

    // cleanup
    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_escalation();
    return 0;
}
