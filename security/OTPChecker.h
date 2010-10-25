/**
 * @file OTPChecker.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * @section DESCRIPTION
 * 
 * Object that checks OTPs agains known seeds.
 */

#ifndef BADVPN_SECURITY_OTPCHECKER_H
#define BADVPN_SECURITY_OTPCHECKER_H

#include <stdint.h>

#include <misc/balign.h>
#include <misc/debug.h>
#include <misc/modadd.h>
#include <security/OTPCalculator.h>
#include <system/DebugObject.h>

struct OTPChecker_entry {
    otp_t otp;
    int avail;
};

#include <generated/bstruct_OTPChecker.h>

/**
 * Object that checks OTPs agains known seeds.
 */
typedef struct {
    DebugObject d_obj;
    int num_otps;
    int num_entries;
    int num_tables;
    int tables_used;
    int next_table;
    OTPCalculator calc;
    oc_tablesParams tables_params;
    oc_tables *tables;
} OTPChecker;

/**
 * Initializes the checker.
 *
 * @param mc the object
 * @param num_otps number of OTPs to generate from a seed. Must be >0.
 * @param cipher encryption cipher for calculating the OTPs. Must be valid
 *               according to {@link BEncryption_cipher_valid}.
 * @param num_tables number of tables to keep, each for one seed. Must be >0.
 * @return 1 on success, 0 on failure
 */
static int OTPChecker_Init (OTPChecker *mc, int num_otps, int cipher, int num_tables) WARN_UNUSED;

/**
 * Frees the checker.
 *
 * @param mc the object
 */
static void OTPChecker_Free (OTPChecker *mc);

/**
 * Adds a seed whose OTPs should be recognized.
 *
 * @param mc the object
 * @param seed_id seed identifier
 * @param key encryption key
 * @param iv initialization vector
 */
static void OTPChecker_AddSeed (OTPChecker *mc, uint16_t seed_id, uint8_t *key, uint8_t *iv);

/**
 * Removes all active seeds.
 *
 * @param mc the object
 */
static void OTPChecker_RemoveSeeds (OTPChecker *mc);

/**
 * Checks an OTP.
 *
 * @param mc the object
 * @param seed_id identifer of seed whom the OTP is claimed to belong to
 * @param otp OTP to check
 * @return 1 if the OTP is valid, 0 if not
 */
static int OTPChecker_CheckOTP (OTPChecker *mc, uint16_t seed_id, otp_t otp);

static void OTPChecker_Table_Empty (OTPChecker *mc, oc_table *t);
static void OTPChecker_Table_AddOTP (OTPChecker *mc, oc_table *t, otp_t otp);
static void OTPChecker_Table_Generate (OTPChecker *mc, oc_table *t, OTPCalculator *calc, uint8_t *key, uint8_t *iv);
static int OTPChecker_Table_CheckOTP (OTPChecker *mc, oc_table *t, otp_t otp);

void OTPChecker_Table_Empty (OTPChecker *mc, oc_table *t)
{
    for (int i = 0; i < mc->num_entries; i++) {
        oc_table_entries_at(&mc->tables_params.tables_params, t, i)->avail = -1;
    }
}

void OTPChecker_Table_AddOTP (OTPChecker *mc, oc_table *t, otp_t otp)
{
    // calculate starting index
    int start_index = otp % mc->num_entries;
    
    // try indexes starting with the base position
    for (int i = 0; i < mc->num_entries; i++) {
        int index = BMODADD(start_index, i, mc->num_entries);
        struct OTPChecker_entry *entry = oc_table_entries_at(&mc->tables_params.tables_params, t, index);
        
        // if we find a free index, use it
        if (entry->avail < 0) {
            entry->otp = otp;
            entry->avail = 1;
            return;
        }
        
        // if we find a used index with the same mac,
        // use it by incrementing its count
        if (entry->otp == otp) {
            entry->avail++;
            return;
        }
    }
    
    // will never add more macs than we can hold
    ASSERT(0)
}

void OTPChecker_Table_Generate (OTPChecker *mc, oc_table *t, OTPCalculator *calc, uint8_t *key, uint8_t *iv)
{
    // calculate values
    otp_t *otps = OTPCalculator_Generate(calc, key, iv, 0);
    
    // empty table
    OTPChecker_Table_Empty(mc ,t);
    
    // add calculated values to table
    for (int i = 0; i < mc->num_otps; i++) {
        OTPChecker_Table_AddOTP(mc, t, otps[i]);
    }
}

int OTPChecker_Table_CheckOTP (OTPChecker *mc, oc_table *t, otp_t otp)
{
    // calculate starting index
    int start_index = otp % mc->num_entries;
    
    // try indexes starting with the base position
    for (int i = 0; i < mc->num_entries; i++) {
        int index = BMODADD(start_index, i, mc->num_entries);
        struct OTPChecker_entry *entry = oc_table_entries_at(&mc->tables_params.tables_params, t, index);
        
        // if we find an empty entry, there is no such mac
        if (entry->avail < 0) {
            return 0;
        }
        
        // if we find a matching entry, check its count
        if (entry->otp == otp) {
            if (entry->avail > 0) {
                entry->avail--;
                return 1;
            }
            return 0;
        }
    }
    
    // there are always empty slots
    ASSERT(0)
    return 0;
}

int OTPChecker_Init (OTPChecker *mc, int num_otps, int cipher, int num_tables)
{
    ASSERT(num_otps > 0)
    ASSERT(BEncryption_cipher_valid(cipher))
    ASSERT(num_tables > 0)
    
    // init arguments
    mc->num_otps = num_otps;
    mc->num_tables = num_tables;
    
    // set number of entries
    mc->num_entries = 2 * mc->num_otps;
    
    // set no tables used
    mc->tables_used = 0;
    mc->next_table = 0;
    
    // initialize calculator
    if (!OTPCalculator_Init(&mc->calc, mc->num_otps, cipher)) {
        goto fail0;
    }
    
    // allocate tables
    oc_tablesParams_Init(&mc->tables_params, mc->num_tables, mc->num_entries);
    if (!(mc->tables = malloc(mc->tables_params.len))) {
        goto fail1;
    }
    
    // initialize tables
    for (int i = 0; i < mc->num_tables; i++) {
        OTPChecker_Table_Empty(mc, oc_tables_tables_at(&mc->tables_params, mc->tables, i));
    }
    
    // init debug object
    DebugObject_Init(&mc->d_obj);
    
    return 1;
    
fail1:
    OTPCalculator_Free(&mc->calc);
fail0:
    return 0;
}

void OTPChecker_Free (OTPChecker *mc)
{
    // free debug object
    DebugObject_Free(&mc->d_obj);
    
    // free tables
    free(mc->tables);
    
    // free calculator
    OTPCalculator_Free(&mc->calc);
}

void OTPChecker_AddSeed (OTPChecker *mc, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(mc->next_table >= 0)
    ASSERT(mc->next_table < mc->num_tables)
    
    // initialize next table
    oc_table *table = oc_tables_tables_at(&mc->tables_params, mc->tables, mc->next_table);
    *oc_table_id(&mc->tables_params.tables_params, table) = seed_id;
    OTPChecker_Table_Generate(mc, table, &mc->calc, key, iv);
    
    // update next table number
    mc->next_table = BMODADD(mc->next_table, 1, mc->num_tables);
    // update number of used tables if not all are used yet
    if (mc->tables_used < mc->num_tables) {
        mc->tables_used++;
    }
}

void OTPChecker_RemoveSeeds (OTPChecker *mc)
{
    mc->tables_used = 0;
    mc->next_table = 0;
}

int OTPChecker_CheckOTP (OTPChecker *mc, uint16_t seed_id, otp_t otp)
{
    // try tables in reverse order
    for (int i = 1; i <= mc->tables_used; i++) {
        int table_index = BMODADD(mc->next_table, mc->num_tables - i, mc->num_tables);
        oc_table *table = oc_tables_tables_at(&mc->tables_params, mc->tables, table_index);
        if (*oc_table_id(&mc->tables_params.tables_params, table) == seed_id) {
            return OTPChecker_Table_CheckOTP(mc, table, otp);
        }
    }
    
    return 0;
}

#endif
