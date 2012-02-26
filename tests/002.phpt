--TEST--
add_around simple
--FILE--
<?php 

class mytest {
	public function test () {
		echo "intest";
	}
}

mygale_add_around("mytest::test", function ($pObj) {echo "before"; $pObj->process(); echo "after"; });
$test = new mytest();
$test->test();

?>
--EXPECT--
beforeintestafter
