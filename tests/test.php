<?php
class test_class {

	public $data = "toto";
	function test () {
		echo "TEST - ".$this->data."\n";
		return "ret";
	}

	function toto () {
		echo "TOTO\n";
		return "test";
	}
}




$my = new test_class();
//AOP_start();
//*
AOP_add_around("test_class::test",
function ($pObj) {
//		throw new Exception("around");
	echo "Start Time\n";
	$time = microtime(true);
	$pObj->process();
/*
	echo $pObj->getFunctionName()." take ".(microtime(true)-$time)."\n";
	return "{".$ret."}";*/
});
//*/
//*
AOP_add_around("test_class::test2",
function ($pObj) {
		//throw new Exception("around");
	echo "Start Time 2\n";
	$time = microtime(true);
//	$ret = $pObj->process();
	//var_dump($ret);
	echo $pObj->getFunctionName()." 2 take ".(microtime(true)-$time)."\n";
	return "[".$ret."]";
});
//*/
//$my->test();
$my->data="----";
	$toto = $my->test("TEST");
//var_dump($toto);
//var_dump($my->test("TEST"));
$my->toto();
echo "???";
exit;

















//AOP_add_before("test_class::test",function ($pObj) { echo "before ".$pObj->getFunctionName()."\n";$pObj->process(); });
//AOP_add_before("test_class::toto",function ($pObj) { echo "before ".$pObj->getFunctionName()."\n"; });
//AOP_add_around("test_class::toto",function ($pObj) { echo "before ".$pObj->getFunctionName()."\n";$pObj->process(); });
//AOP_add_around("test_class::test",function ($pObj) { echo "before ".$pObj->getFunctionName()."\n";$pObj->process(); });
$my->test("arg1","arg2");
$my->toto("arg1","arg2");
$my->titi("arg1","arg2");
AOP_stop ();
//$test = new AOPPC();
//var_dump( $test->getArgs());
