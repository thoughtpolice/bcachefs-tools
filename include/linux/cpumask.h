#ifndef __LINUX_CPUMASK_H
#define __LINUX_CPUMASK_H

#define num_online_cpus()	1U
#define num_possible_cpus()	1U
#define num_present_cpus()	1U
#define num_active_cpus()	1U
#define cpu_online(cpu)		((cpu) == 0)
#define cpu_possible(cpu)	((cpu) == 0)
#define cpu_present(cpu)	((cpu) == 0)
#define cpu_active(cpu)		((cpu) == 0)

#define for_each_cpu(cpu, mask)			\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)
#define for_each_cpu_not(cpu, mask)		\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)
#define for_each_cpu_and(cpu, mask, and)	\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask, (void)and)

#define for_each_possible_cpu(cpu) for_each_cpu((cpu), 1)
#define for_each_online_cpu(cpu)   for_each_cpu((cpu), 1)
#define for_each_present_cpu(cpu)  for_each_cpu((cpu), 1)

#endif /* __LINUX_CPUMASK_H */
