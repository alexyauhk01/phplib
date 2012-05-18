<?php

##
## Copyright (c) 1998-2000 NetUSE AG
##                    Boris Erdmann, Kristian Koehntopp
##
## Copyright (c) 1998-2000 Sascha Schumann <sascha@schumann.cx>
## 
## $Id: ct_sql.inc,v 1.4 2001/05/17 00:54:20 chrisj Exp $
##
## PHPLIB Data Storage Container using a SQL database
##

class CT_Sql {
  ##
  ## Define these parameters by overwriting or by
  ## deriving your own class from it (recommened)
  ##
    
  var $database_table = "active_sessions";
  var $database_class = "DB_Sql";
  var $database_lock_semaphore = "";
  var $database_engine = "Memory";

  var $encoding_mode = "base64";

  ## end of configuration

  var $db;

  function ac_start() {
    $name = $this->database_class;
    $this->db = new $name;
  }

  function ac_get_lock() {
    if ( "" != $this->database_lock_semaphore ) {
      $query = sprintf("SELECT get_lock('%s')", $this->database_lock_semaphore);
      while ( ! $this->db->query($query)) {
        $t = 1 + time(); while ( $t > time() ) { ; }
      }
    }
  }

  function ac_release_lock() {
    if ( "" != $this->database_lock_semaphore ) {
      $query = sprintf("SELECT release_lock('%s')", $this->database_lock_semaphore);
      $this->db->query($query);
    }
  }

  function ac_gc($gc_time, $name) {
    $timeout = time();
    $sqldate = date("YmdHis", $timeout - ($gc_time * 60));
    $this->db->query(sprintf("DELETE FROM %s WHERE changed < '%s' AND name = '%s'",
                    $this->database_table, 
                    $sqldate,
                    addslashes($name)));
    }

  function ac_store($id, $name, $str) {
    $ret = true;

    switch ( $this->encoding_mode ) {
      case "slashes":
        $str = addslashes($name . ":" . $str);
      break;

      case "base64":
      default:
        $str = base64_encode($name . ":" . $str);
    };

    $name = addslashes($name);

    ## update duration of visit
    global $HTTP_REFERER, $HTTP_USER_AGENT, $REMOTE_ADDR, $auth;

    $username = addslashes( empty($auth->auth["uname"]) ? "unknown" : $auth->auth["uname"]);

    $now = date("YmdHis", time());
    $uquery = sprintf("update %s set val=compress('%s'), changed='%s' where sid='%s' and name='%s'",
      $this->database_table,
      $str,
      $now,
      $id,
      $name);
    $squery = sprintf("select count(*) from `%s` where val=compress('%s') and changed='%s' and sid='%s' and name='%s'",
      $this->database_table,
      $str,
      $now,
      $id,
      $name);
    $iquery = sprintf("insert into %s ( sid, name, val, changed ) values ('%s', '%s', compress('%s'), '%s')",
      $this->database_table,
      $id,
      $name,
      $str,
      $now);
    $rquery = sprintf("replace into %s ( sid, name, val, changed, username ) values ('%s', '%s', compress('%s'), '%s', '%s')",
      $this->database_table,
      $id,
      $name,
      $str,
      $now,
      $username);

    if (true) {
      $this->db->query($rquery);
      return true;
    }

    $this->db->query($uquery);
    # FIRST test to see if any rows were affected.
    #   Zero rows affected could mean either there were no matching rows
    #   whatsoever, OR that the update statement did match a row but made
    #   no changes to the table data (i.e. UPDATE tbl SET col = 'x', when
    #   "col" is _already_ set to 'x') so then,
    # SECOND, query(SELECT...) on the sid to determine if the row is in
    #   fact there,
    # THIRD, verify that there is at least one row present, and if there
    #   is not, then
    # FOURTH, insert the row as we've determined that it does not exist.
 
    if ( $this->db->affected_rows() == 0
        && $this->db->query($squery)
	&& $this->db->next_record() && $this->db->f(0) == 0
        && !$this->db->query($iquery)) {

        $ret = false;
    }
    return $ret;
  }

  function ac_delete($id, $name) {
    $this->db->query(sprintf("delete from %s where name = '%s' and sid = '%s'",
      $this->database_table,
      addslashes($name),
      $id));
  }

  function ac_get_value($id, $name) {
    $cquery = sprintf("
      CREATE TABLE IF NOT EXISTS `%s` (
        `sid` varchar(32) NOT NULL default '',
        `name` varchar(32) NOT NULL default '',
        `val` varbinary(10000) default NULL,
        `changed` varchar(14) NOT NULL default '',
        PRIMARY KEY  (`name`,`sid`),
        KEY `changed` (`changed`)
      ) ENGINE=%s DEFAULT CHARSET=latin1;",
      $this->database_table,
      $this->database_engine);
    $PrevState = $this->db->Halt_On_Error;
    $this->db->Halt_On_Error = "no";
    $this->db->query(sprintf("select uncompress(val) as cval, val from %s where sid  = '%s' and name = '%s'",
      $this->database_table,
      $id,
      addslashes($name)));
    $this->db->Halt_On_Error = $PrevState;
    if ($this->db->Errno == 1146) {
      /* When using Memory Engine the sessions table disappears when the server reboots so we re-create it */
      $this->db->query($cquery);
    } else
    if ($this->db->next_record()) {
      $val  = $this->db->f("val");
      $cval = $this->db->f("cval");
      $str = $cval ? $cval : $val;
      $str2 = base64_decode( $str );

      //if ( ereg("^".$name.":.*", $str2) ) {
      if ( preg_match("/^".$name.":.*/", $str2) ) {
         //$str = ereg_replace("^".$name.":", "", $str2 );
         $str = preg_replace("/^".$name.":/", "", $str2 );
      } else {

        $str3 = stripslashes( $str );

        if ( preg_match("/^".$name.":.*/", $str3) ) {
          $str = preg_replace("/^".$name.":/", "", $str3 );
        } else {

          switch ( $this->encoding_mode ) {
            case "slashes":
              $str = stripslashes($str);
            break;

            case "base64":
            default:
              $str = base64_decode($str);
          }
        }
      };
      return $str;
    };
    return "";
  }

  function ac_newid($str, $name) {
    return $str;
  }

  function ac_halt($s) {
    $this->db->halt($s);
  }
}
?>