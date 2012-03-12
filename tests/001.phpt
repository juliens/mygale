--TEST--
AOP_add_before simple test echo
--FILE--
<?php 

class mytest {
	public function test () {
		echo "intest";
	}
}

AOP_add_before("mytest::test", function () { echo "before"; });
$test = new mytest();
$test->test();

?>
--EXPECT--
beforeintest
