--TEST--
Joker
--FILE--
<?php 

function test () {
	return "intest";
}

mygale_add_around("test", function ($pObj) {return "[".$pObj->process()."]";});
echo test();

?>
--EXPECT--
[intest]
