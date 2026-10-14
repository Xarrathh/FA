<?php
/**********************************************************************
    [CUSTOM] Bank statement import — allocation rules (native FA core).
	Stores recurring description->GL-account mappings used to
	auto-allocate imported bank transactions before posting.

	Table (per company prefix): `<prefix>_bank_import_rules`
	  id          int auto
	  pattern     varchar(100)  — case-insensitive substring of the
	                              bank transaction description
	  gl_account  int           — chart_master.account_code (offset)
	  trans_type  tinyint       — 1=payment, 2=deposit, 0=either
	  inactive    tinyint       — 1 = disabled
**********************************************************************/

function bank_import_rules_table_exists()
{
	global $db_connections;
	return @db_num_rows(db_query("SHOW TABLES LIKE '".TB_PREF."bank_import_rules'")) == 1;
}

function bank_import_ensure_rules_table()
{
	// self-healing: create the table if this company predates the feature
	$sql = "CREATE TABLE IF NOT EXISTS ".TB_PREF."bank_import_rules (
		id INT(11) NOT NULL AUTO_INCREMENT,
		pattern VARCHAR(100) NOT NULL DEFAULT '',
		gl_account INT(11) NOT NULL DEFAULT 0,
		trans_type TINYINT(4) NOT NULL DEFAULT 0,
		inactive TINYINT(4) NOT NULL DEFAULT 0,
		PRIMARY KEY (id),
		KEY pattern (pattern)
	) ENGINE=InnoDB DEFAULT CHARSET=utf8";
	db_query($sql, _("Cannot create bank import rules table"));
}

function get_bank_import_rules($show_inactive=false)
{
	bank_import_ensure_rules_table();
	$sql = "SELECT r.*, a.account_name
		FROM ".TB_PREF."bank_import_rules r
		LEFT JOIN ".TB_PREF."chart_master a ON a.account_code=r.gl_account";
	if (!$show_inactive)
		$sql .= " WHERE r.inactive=0";
	$sql .= " ORDER BY r.pattern";
	return db_query($sql, _("Cannot query bank import rules"));
}

function add_bank_import_rule($pattern, $gl_account, $trans_type=0)
{
	$sql = "INSERT INTO ".TB_PREF."bank_import_rules (pattern, gl_account, trans_type)
		VALUES (".db_escape($pattern).", ".db_escape($gl_account).", ".db_escape($trans_type).")";
	db_query($sql, _("Cannot insert bank import rule"));
	return db_insert_id();
}

function update_bank_import_rule($id, $pattern, $gl_account, $trans_type, $inactive)
{
	$sql = "UPDATE ".TB_PREF."bank_import_rules
		SET pattern=".db_escape($pattern).",
			gl_account=".db_escape($gl_account).",
			trans_type=".db_escape($trans_type).",
			inactive=".db_escape($inactive)."
		WHERE id=".db_escape($id);
	db_query($sql, _("Cannot update bank import rule"));
}

function delete_bank_import_rule($id)
{
	db_query("DELETE FROM ".TB_PREF."bank_import_rules WHERE id=".db_escape($id),
		_("Cannot delete bank import rule"));
}

// Return the matching rule (or null) for a description + direction.
// $trans_type: 1=payment, 2=deposit. A rule matches if its trans_type is
// 0 (either) or equals the transaction's type, and its pattern appears
// in the description (case-insensitive).
function bank_import_find_rule($description, $trans_type)
{
	bank_import_ensure_rules_table();
	$sql = "SELECT gl_account, pattern
		FROM ".TB_PREF."bank_import_rules
		WHERE inactive=0 AND (trans_type=0 OR trans_type=".db_escape($trans_type).")
		ORDER BY LENGTH(pattern) DESC";
	$res = db_query($sql, _("Cannot query bank import rules"));
	$desc = mb_strtolower(trim($description));
	while ($row = db_fetch($res)) {
		if ($row['pattern'] !== '' && mb_strpos($desc, mb_strtolower($row['pattern'])) !== false)
			return $row;
	}
	return null;
}
