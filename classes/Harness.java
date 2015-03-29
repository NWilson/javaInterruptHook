public class Harness {

	public static void main(String[] args) {
		final NativeTask t = new NativeTask();
		final Thread thr = Thread.currentThread();
		(new Thread(new Runnable() {
			public void run() {
				try {
					Thread.sleep(3000);
				} catch (InterruptedException e) {}
				//t.wakeupTask();
				thr.interrupt();
			}
		})).start();
		try {
			t.doTask();
			System.out.println("Task returned normally");
		} catch (InterruptedException e) {
			System.out.println("Thread interrupted!");
		}
	}

}
